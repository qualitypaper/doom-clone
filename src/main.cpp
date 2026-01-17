#include "framebuffer.h"

int main()
{
    framebuffer::FrameBuffer *fb = new framebuffer::FrameBuffer();

    uint8_t r = 255, g = 255, b = 0, alpha = 255;
    uint32_t yellow = (r << 24) | (g << 16) | (b << 8) | alpha;

    std::cout << "Set up window" << '\n';

    for (int i = 0; i < config::CANVAS_WIDTH; ++i)
    {
        for (int j = 0; j < config::CANVAS_HEIGHT; ++j)
        {
            (*fb).color(glm::vec2{i,j}, yellow);
        }
    }

    std::cout << "Colored with yellow" << '\n';
    (*fb).update();

    system("sleep 5");

    delete fb;

    return 0;
}