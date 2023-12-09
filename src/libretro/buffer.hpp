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

#ifndef MELONDS_DS_BUFFER_HPP
#define MELONDS_DS_BUFFER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

#include <glm/vec2.hpp>

namespace MelonDsDs {
    class PixelBuffer {
    public:
        PixelBuffer(unsigned width, unsigned height) noexcept;
        PixelBuffer(glm::uvec2 size) noexcept;
        PixelBuffer(const PixelBuffer&) noexcept;
        PixelBuffer(PixelBuffer&&) noexcept;
        PixelBuffer& operator=(const PixelBuffer&) noexcept;
        PixelBuffer& operator=(PixelBuffer&&) noexcept;

        [[nodiscard]] uint32_t operator[](glm::uvec2 pos) const noexcept {
            return buffer[pos.y * size.x + pos.x];
        }

        [[nodiscard]] uint32_t& operator[](glm::uvec2 pos) noexcept {
            return buffer[pos.y * size.x + pos.x];
        }

        [[nodiscard]] uint32_t* operator[](unsigned row) noexcept {
            return buffer.get() + row * size.x;
        }

        [[nodiscard]] const uint32_t* operator[](unsigned row) const noexcept {
            return buffer.get() + row * size.x;
        }

        operator bool() const noexcept { return buffer != nullptr; }

        glm::uvec2 Size() const noexcept { return size; }
        unsigned Width() const noexcept { return size.x; }
        unsigned Height() const noexcept { return size.y; }
        unsigned Stride() const noexcept { return stride; }
        uint32_t *Buffer() noexcept { return buffer.get(); }
        const uint32_t *Buffer() const noexcept { return buffer.get(); }
        void Clear() noexcept;
        void CopyDirect(const uint32_t* source, glm::uvec2 destination) noexcept;
        void CopyRows(const uint32_t* source, glm::uvec2 destination, glm::uvec2 destinationSize) noexcept;
    private:
        glm::uvec2 size;
        unsigned stride;
        std::unique_ptr<uint32_t[]> buffer;
    };
}

#endif //MELONDS_DS_BUFFER_HPP
