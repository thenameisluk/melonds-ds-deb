/*
    Copyright 2023 Jesse Talavera-Greenberg

    melonDS DS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS DS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS DS. If not, see http://www.gnu.org/licenses/.
*/

#include "libretro.hpp"

#include <ctime>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <compat/strl.h>
#include <file/file_path.h>
#include <libretro.h>
#include <streams/file_stream.h>

#include <NDS.h>
#include <NDSCart.h>
#include <frontend/FrontendUtil.h>
#include <Platform.h>
#include <frontend/qt_sdl/Config.h>
#include <GPU.h>
#include <SPU.h>
#include <GBACart.h>
#include <retro_assert.h>

#include "opengl.hpp"
#include "environment.hpp"
#include "config.hpp"
#include "input.hpp"
#include "utils.hpp"
#include "info.hpp"
#include "screenlayout.hpp"
#include "memory.hpp"
#include "render.hpp"
#include "exceptions.hpp"

using std::optional;

using NDSCart::NDSCartData;
using GBACart::GBACartData;

namespace melonds {
    static bool swap_screen_toggled = false;
    static bool deferred_initialization_pending = false;
    static bool first_frame_run = false;
    static std::unique_ptr<NDSCartData> _loaded_nds_cart;
    static std::unique_ptr<GBACartData> _loaded_gba_cart;
    static const char *const INTERNAL_ERROR_MESSAGE =
        "An internal error occurred with melonDS DS. "
        "Please contact the developer with the log file.";

    static const char *const UNKNOWN_ERROR_MESSAGE =
        "An unknown error has occurred with melonDS DS. "
        "Please contact the developer with the log file.";

    // functions for loading games
    static bool handle_load_game(unsigned type, const struct retro_game_info *info, size_t num) noexcept;
    static bool load_games(
        const optional<retro_game_info> &nds_info,
        const optional<retro_game_info> &gba_info
    );
    static void init_firmware_overrides();
    static void parse_nds_rom(const struct retro_game_info &info);
    static void init_nds_save(const NDSCartData &nds_cart);
    static void parse_gba_rom(const struct retro_game_info &info);
    static void init_bios();
    static void init_rendering();
    static bool load_game_deferred(
        const optional<retro_game_info> &nds_info,
        const optional<retro_game_info> &gba_info
    );
    static void set_up_direct_boot(const retro_game_info &nds_info);

    // functions for running games
    static void render_frame();
    static void render_audio();
}

PUBLIC_SYMBOL void retro_init(void) {
    retro::log(RETRO_LOG_DEBUG, "retro_init");

    srand(time(nullptr));

    Platform::Init(0, nullptr);
    melonds::first_frame_run = false;
    // ScreenLayoutData is initialized in its constructor
}

static bool melonds::handle_load_game(unsigned type, const struct retro_game_info *info, size_t num) noexcept try {
    // First initialize the content info...
    switch (type) {
        case melonds::MELONDSDS_GAME_TYPE_NDS:
            // ...which refers to a Nintendo DS game...
            retro::set_loaded_content_info(info, nullptr);
            break;
        case melonds::MELONDSDS_GAME_TYPE_SLOT_1_2_BOOT:
            // ...which refers to both a Nintendo DS and Game Boy Advance game...
            if (num < 2) {
                retro::log(RETRO_LOG_ERROR, "Invalid number of ROMs for slot-1/2 boot");
                retro::set_error_message(melonds::INTERNAL_ERROR_MESSAGE);
                return false;
            }
            retro::set_loaded_content_info(info, (info == nullptr) ? nullptr : info + 1);
            break;
        default:
            retro::log(RETRO_LOG_ERROR, "Unknown game type %d", type);
            retro::set_error_message(melonds::INTERNAL_ERROR_MESSAGE);
            return false;
    }

    // ...then load the game.
    return melonds::load_games(retro::get_loaded_nds_info(), retro::get_loaded_gba_info());
}
catch (const melonds::invalid_rom_exception &e) {
    // Thrown for invalid ROMs
    retro::set_error_message(e.what());
    _loaded_nds_cart.reset();
    _loaded_gba_cart.reset();
    return false;
}
catch (const std::exception &e) {
    retro::log(RETRO_LOG_ERROR, "%s", e.what());
    retro::set_error_message(melonds::INTERNAL_ERROR_MESSAGE);
    _loaded_nds_cart.reset();
    _loaded_gba_cart.reset();
    return false;
}
catch (...) {
    retro::set_error_message(melonds::UNKNOWN_ERROR_MESSAGE);
    _loaded_nds_cart.reset();
    _loaded_gba_cart.reset();
    return false;
}

PUBLIC_SYMBOL bool retro_load_game(const struct retro_game_info *info) {
    retro::log(RETRO_LOG_DEBUG, "retro_load_game(\"%s\", %d)\n", info->path, info->size);

    return melonds::handle_load_game(melonds::MELONDSDS_GAME_TYPE_NDS, info, 1);
}

PUBLIC_SYMBOL void retro_run(void) {
    using namespace melonds;
    using retro::log;
    using Config::Retro::CurrentRenderer;

    if (deferred_initialization_pending) {
        log(RETRO_LOG_DEBUG, "Starting deferred initialization");
        bool game_loaded = melonds::load_game_deferred(retro::get_loaded_nds_info(), retro::get_loaded_gba_info());
        deferred_initialization_pending = false;
        if (!game_loaded) {
            // If we couldn't load the game...
            log(RETRO_LOG_ERROR, "Deferred initialization failed; exiting core");
            retro::environment(RETRO_ENVIRONMENT_SHUTDOWN, nullptr);
            return;
        }
        log(RETRO_LOG_DEBUG, "Completed deferred initialization");
    }

    if (!first_frame_run) {
        if (NdsSaveManager->SramLength() > 0) {
            NDS::LoadSave(NdsSaveManager->Sram(), NdsSaveManager->SramLength());
        }

        if (GbaSaveManager->SramLength() > 0) {
            GBACart::LoadSave(GbaSaveManager->Sram(), GbaSaveManager->SramLength());
        }

        // This has to be deferred even if we're not using OpenGL,
        // because libretro doesn't set the SRAM until after retro_load_game
        first_frame_run = true;
    }

    melonds::update_input(input_state);

    if (melonds::input_state.swap_screens_btn != Config::ScreenSwap) {
        switch (Config::Retro::ScreenSwapMode) {
            case melonds::ScreenSwapMode::Toggle: {
                if (!Config::ScreenSwap) {
                    swap_screen_toggled = !swap_screen_toggled;
                    update_screenlayout(current_screen_layout(), &screen_layout_data,
                                        CurrentRenderer == Renderer::OpenGl,
                                        swap_screen_toggled);
                    melonds::opengl::RequestOpenGlRefresh();
                }

                Config::ScreenSwap = input_state.swap_screens_btn;
                log(RETRO_LOG_DEBUG, "Toggled screen-swap mode (now %s)", Config::ScreenSwap ? "on" : "off");
                break;
            }
            case ScreenSwapMode::Hold: {
                if (Config::ScreenSwap != input_state.swap_screens_btn) {
                    log(RETRO_LOG_DEBUG, "%s holding the screen-swap button",
                        input_state.swap_screens_btn ? "Started" : "Stopped");
                }
                Config::ScreenSwap = input_state.swap_screens_btn;
                update_screenlayout(current_screen_layout(), &screen_layout_data,
                                    CurrentRenderer == Renderer::OpenGl,
                                    Config::ScreenSwap);
                melonds::opengl::RequestOpenGlRefresh();
            }
        }
    }

    auto mic_input_mode = static_cast<MicInputMode>(Config::MicInputType);

    if (Config::Retro::MicButtonRequired && !input_state.holding_noise_btn) {
        mic_input_mode = melonds::MicInputMode::None;
    }

    switch (mic_input_mode) {
        case MicInputMode::WhiteNoise: // random noise
        {
            s16 tmp[735];
            for (int i = 0; i < 735; i++) tmp[i] = rand() & 0xFFFF;
            NDS::MicInputFrame(tmp, 735);
            break;
        }
        case MicInputMode::BlowNoise: // blow noise
        {
            Frontend::Mic_FeedNoise(); // despite the name, this feeds a blow noise
            break;
        }
        case MicInputMode::HostMic: // microphone input
        {
            s16 tmp[735];
//                if (micHandle && micInterface.interface_version &&
//                    micInterface.get_mic_state(micHandle)) { // If the microphone is enabled and supported...
//                    micInterface.read_mic(micHandle, tmp, 735);
//                    NDS::MicInputFrame(tmp, 735);
//                    break;
//                } // If the mic isn't available, go to the default case
        }
        default:
            Frontend::Mic_FeedSilence();
    }

    if (melonds::render::ReadyToRender()) { // If the global state needed for rendering is ready...
        // NDS::RunFrame invokes rendering-related code
        NDS::RunFrame();

        // TODO: Use RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE
        melonds::render_frame();
        melonds::render_audio();
    }

    bool updated = false;
    if (retro::environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        melonds::check_variables(false);

        struct retro_system_av_info updated_av_info{};
        retro_get_system_av_info(&updated_av_info);
        retro::environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &updated_av_info);
        screen_layout_data.clean_screenlayout_buffer();
    }
}

static void melonds::render_frame() {
    switch (Config::Retro::CurrentRenderer) {
#ifdef HAVE_OPENGL
        case Renderer::OpenGl:
            melonds::opengl::render_frame();
            break;
#endif
        case Renderer::Software:
        default:
            render::RenderSoftware();
            break;
    }
}

static void melonds::render_audio() {
    static int16_t audio_buffer[0x1000]; // 4096 samples == 2048 stereo frames
    u32 size = std::min(SPU::GetOutputSize(), static_cast<int>(sizeof(audio_buffer) / (2 * sizeof(int16_t))));
    // Ensure that we don't overrun the buffer

    size_t read = SPU::ReadOutput(audio_buffer, size);
    retro::audio_sample_batch(audio_buffer, read);
}

PUBLIC_SYMBOL void retro_unload_game(void) {
    retro::log(RETRO_LOG_DEBUG, "retro_unload_game()");
    // TODO: If this is homebrew, save the data
    // No need to flush SRAM, Platform::WriteNDSSave has been doing that for us this whole time
    NDS::Stop();
    NDS::DeInit();
    melonds::_loaded_nds_cart.reset();
    melonds::_loaded_gba_cart.reset();
}

PUBLIC_SYMBOL unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

PUBLIC_SYMBOL bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    retro::log(RETRO_LOG_DEBUG, "retro_load_game_special(%s, %p, %u)", melonds::get_game_type_name(type), info, num);

    return melonds::handle_load_game(type, info, num);
}

PUBLIC_SYMBOL void retro_deinit(void) {
    retro::log(RETRO_LOG_DEBUG, "retro_deinit()");
    retro::clear_environment();
    melonds::clear_memory_config();
    melonds::_loaded_nds_cart.reset();
    melonds::_loaded_gba_cart.reset();
    Platform::DeInit();
}

PUBLIC_SYMBOL unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

PUBLIC_SYMBOL void retro_get_system_info(struct retro_system_info *info) {
    info->library_name = "melonDS DS";
    info->block_extract = false;
    info->library_version = "TODO: Version number";
    info->need_fullpath = false;
    info->valid_extensions = "nds|ids|dsi";
}

PUBLIC_SYMBOL void retro_reset(void) {
    retro::log(RETRO_LOG_DEBUG, "retro_reset()\n");
    NDS::Reset();

    melonds::first_frame_run = false;

    const auto &nds_info = retro::get_loaded_nds_info();
    if (nds_info) {
        melonds::set_up_direct_boot(nds_info.value());
    }
}

static void melonds::parse_nds_rom(const struct retro_game_info &info) {
    _loaded_nds_cart = std::make_unique<NDSCartData>(
        static_cast<const u8 *>(info.data),
        static_cast<u32>(info.size)
    );

    if (!_loaded_nds_cart->IsValid()) {
        throw invalid_rom_exception("Failed to parse the DS ROM image. Is it valid?");
    }

    retro::log(RETRO_LOG_DEBUG, "Loaded NDS ROM: \"%s\"", info.path);
}

static void melonds::parse_gba_rom(const struct retro_game_info &info) {
    if (Config::ConsoleType == ConsoleType::DSi) {
        retro::set_warn_message("The DSi does not support GBA connectivity. Not loading the requested GBA ROM.");
    } else {
        _loaded_gba_cart = std::make_unique<GBACartData>(
            static_cast<const u8 *>(info.data),
            static_cast<u32>(info.size)
        );

        if (!_loaded_gba_cart->IsValid()) {
            throw invalid_rom_exception("Failed to parse the GBA ROM image. Is it valid?");
        }

        retro::log(RETRO_LOG_DEBUG, "Loaded GBA ROM: \"%s\"", info.path);
    }
}

// TODO: Support loading the firmware without a ROM
// TODO: Support loading a specified GBA save file
static bool melonds::load_games(
    const optional<struct retro_game_info> &nds_info,
    const optional<struct retro_game_info> &gba_info
) {
    melonds::clear_memory_config();
    melonds::check_variables(true);

    using retro::environment;
    using retro::log;
    using retro::set_message;

    /*
    * NDS::Reset() calls wipes the cart buffer so on invoke we need a reload from info->data.
    * Since retro_reset callback doesn't pass the info struct we need to cache it.
    */

    retro_assert(_loaded_nds_cart == nullptr);
    retro_assert(_loaded_gba_cart == nullptr);

    init_firmware_overrides();

    // First parse the ROMs...
    if (nds_info) {
        parse_nds_rom(*nds_info);

        // sanity check; parse_nds_rom does the real validation
        retro_assert(_loaded_nds_cart != nullptr);
        retro_assert(_loaded_nds_cart->IsValid());

        init_nds_save(*_loaded_nds_cart);
    }

    if (gba_info) {
        parse_gba_rom(*gba_info);
    }

    init_bios();
    environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void *) &melonds::input_descriptors);

    init_rendering();

    if (!NDS::Init()) {
        retro::log(RETRO_LOG_ERROR, "Failed to initialize melonDS DS.");
        return false;
    }

    SPU::SetInterpolation(Config::AudioInterp);
    NDS::SetConsoleType(Config::ConsoleType);

    if (Config::Retro::CurrentRenderer == Renderer::OpenGl) {
        log(RETRO_LOG_INFO, "Deferring initialization until the OpenGL context is ready");
        deferred_initialization_pending = true;
        return true;
    } else {
        log(RETRO_LOG_INFO, "No need to defer initialization, proceeding now");
        return load_game_deferred(nds_info, gba_info);
    }
}


static void melonds::init_firmware_overrides() {
    // TODO: Ensure that the username is non-empty
    // TODO: Make firmware overrides configurable
    // TODO: Cap the username to match the DS's limit (10 chars, excluding null terminator)

    const char *retro_username;
    if (retro::environment(RETRO_ENVIRONMENT_GET_USERNAME, &retro_username) && retro_username)
        Config::FirmwareUsername = retro_username;
    else
        Config::FirmwareUsername = "melonDS";
}

// Does not load the NDS SRAM, since retro_get_memory is used for that.
// But it will allocate the buffer and load homebrew save data.
static void melonds::init_nds_save(const NDSCart::NDSCartData &nds_cart) {
    if (nds_cart.Header().IsHomebrew()) {
        // If this cart is a homebrew ROM...

        // Homebrew is a special case, as it uses an SD card rather than SRAM.
        // TODO: Get the save data path
        // TODO: Load the homebrew save data image
    } else {
        // This is a retail ROM

        // Get the length of the ROM's SRAM, if any
        u32 sram_length = _loaded_nds_cart->Cart()->GetSaveMemoryLength();
        NdsSaveManager->SetSaveSize(sram_length);

        if (sram_length > 0) {
            retro::log(RETRO_LOG_DEBUG, "Allocated %u-byte SRAM buffer for loaded NDS ROM.", sram_length);
        } else {
            retro::log(RETRO_LOG_DEBUG, "Loaded NDS ROM does not use SRAM.");
        }
    }
}

static void melonds::init_bios() {
    using retro::log;

    // TODO: Allow user to force the use of a specific BIOS, and throw an exception if that's not possible
    if (Config::ExternalBIOSEnable) {
        // If the player wants to use their own BIOS dumps...

        // melonDS doesn't properly fall back to FreeBIOS if the external bioses are missing,
        // so we have to do it ourselves

        // TODO: Don't always check all files; just check for the ones we need
        // based on the console type
        std::array<std::string, 3> required_roms = {Config::BIOS7Path, Config::BIOS9Path, Config::FirmwarePath};
        std::vector<std::string> missing_roms;

        // Check if any of the bioses / firmware files are missing
        for (std::string &rom: required_roms) {
            if (Platform::LocalFileExists(rom)) {
                log(RETRO_LOG_INFO, "Found %s", rom.c_str());
            } else {
                missing_roms.push_back(rom);
                log(RETRO_LOG_WARN, "Could not find %s", rom.c_str());
            }
        }

        // TODO: Check both $SYSTEM/filename and $SYSTEM/melonDS DS/filename

        // Abort if there are any of the required roms are missing
        if (!missing_roms.empty()) {
            Config::ExternalBIOSEnable = false;
            retro::log(RETRO_LOG_WARN, "Using FreeBIOS instead of the aforementioned missing files.");
        }
    } else {
        retro::log(RETRO_LOG_INFO, "External BIOS is disabled, using internal FreeBIOS instead.");
    }
}

// melonDS tightly couples the renderer with the rest of the emulation code,
// so we can't initialize the emulator until the OpenGL context is ready.
static bool melonds::load_game_deferred(
    const optional<struct retro_game_info> &nds_info,
    const optional<struct retro_game_info> &gba_info
) {
    using retro::log;

    // GPU config must be initialized before NDS::Reset is called.
    // Ensure that there's a renderer, even if we're about to throw it out.
    // (GPU::SetRenderSettings may try to deinitialize a non-existing renderer)
    GPU::InitRenderer(Config::Retro::CurrentRenderer == Renderer::OpenGl);
    GPU::RenderSettings render_settings = Config::Retro::RenderSettings();
    GPU::SetRenderSettings(Config::Retro::CurrentRenderer == Renderer::OpenGl, render_settings);

    // Loads the BIOS, too
    NDS::Reset();

    // The ROM must be inserted after NDS::Reset is called

    retro_assert(_loaded_nds_cart != nullptr);
    retro_assert(_loaded_nds_cart->IsValid());

    bool inserted = NDSCart::InsertROM(std::move(*_loaded_nds_cart));
    _loaded_nds_cart.reset();
    if (!inserted) {
        // If we failed to insert the ROM, we can't continue
        retro::log(RETRO_LOG_ERROR, "Failed to insert \"%s\" into the emulator. Exiting.", nds_info->path);
        retro::set_error_message("Failed to insert the loaded ROM. Please report this issue.");
        return false;
    }

    if (gba_info && _loaded_gba_cart) {
        inserted = GBACart::InsertROM(std::move(*_loaded_gba_cart));
        _loaded_gba_cart.reset();
        if (!inserted) {
            // If we failed to insert the ROM, we can't continue
            retro::log(RETRO_LOG_ERROR, "Failed to insert \"%s\" into the emulator. Exiting.", gba_info->path);
            retro::set_error_message("Failed to insert the loaded ROM. Please report this issue.");
            return false;
        }
    }

    set_up_direct_boot(nds_info.value());

    NDS::Start();

    log(RETRO_LOG_INFO, "Initialized emulated console and loaded emulated game");

//    micInterface.interface_version = RETRO_MICROPHONE_INTERFACE_VERSION;
//    if (environ_cb(RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE, &micInterface))
//    { // ...and if the current audio driver supports microphones...
//        if (micInterface.interface_version != RETRO_MICROPHONE_INTERFACE_VERSION)
//        {
//            log_cb(RETRO_LOG_WARN, "[melonDS] Expected mic interface version %u, got %u. Compatibility issues are possible.\n",
//                   RETRO_MICROPHONE_INTERFACE_VERSION, micInterface.interface_version);
//        }
//
//        log_cb(RETRO_LOG_DEBUG, "[melonDS] Microphone support available in current audio driver (version %u)\n",
//               micInterface.interface_version);
//
//        retro_microphone_params_t params = {
//                .rate = 44100 // The core engine assumes this rate
//        };
//        micHandle = micInterface.open_mic(&params);
//
//        if (micHandle)
//        {
//            log_cb(RETRO_LOG_INFO, "[melonDS] Initialized microphone\n");
//        }
//        else
//        {
//            log_cb(RETRO_LOG_WARN, "[melonDS] Failed to initialize microphone, emulated device will receive silence\n");
//        }
//    }

    return true;
}

static void melonds::init_rendering() {
    using retro::environment;
    using retro::log;

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        throw std::runtime_error("Failed to set the required XRGB8888 pixel format for rendering; it may not be supported.");
    }

#ifdef HAVE_OPENGL
    // Initialize the opengl state if needed
    switch (Config::Retro::ConfiguredRenderer) {
        // Depending on which renderer we want to use...
        case Renderer::OpenGl:
            if (melonds::opengl::initialize()) {
                Config::Retro::CurrentRenderer = Renderer::OpenGl;
            } else {
                Config::Retro::CurrentRenderer = Renderer::Software;
                log(RETRO_LOG_ERROR, "Failed to initialize OpenGL renderer, falling back to software rendering");
                // TODO: Display a message stating that we're falling back to software rendering
            }
            break;
        default:
            log(RETRO_LOG_WARN, "Unknown renderer %d, falling back to software rendering",
                static_cast<int>(Config::Retro::ConfiguredRenderer));
            // Intentional fall-through
        case Renderer::Software:
            Config::Retro::CurrentRenderer = Renderer::Software;
            log(RETRO_LOG_INFO, "Using software renderer");
            break;
    }
#else
    log(RETRO_LOG_INFO, "OpenGL is not supported by this build, using software renderer");
#endif
}

// Decrypts the ROM's secure area
static void melonds::set_up_direct_boot(const retro_game_info &nds_info) {
    if (Config::DirectBoot || NDS::NeedsDirectBoot()) {
        char game_name[256];
        const char *ptr = path_basename(nds_info.path);
        if (ptr)
            strlcpy(game_name, ptr, sizeof(game_name));
        else
            strlcpy(game_name, nds_info.path, sizeof(game_name));
        path_remove_extension(game_name);

        NDS::SetupDirectBoot(game_name);
        retro::log(RETRO_LOG_DEBUG, "Initialized direct boot for \"%s\"", game_name);
    }
}
