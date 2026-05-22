#pragma once

#include "types_sfml.hpp"

#include <stdexec/execution.hpp>

namespace render {

static auto MakeSfmlDisplaySender(SfmlState &st) {
    static AvrTimeCounter time_counter;
    return ex::then([&](FrameBuffer *fb) {
        time_counter.Start();
        st.texture.update(fb->rgba.data());

        st.window.clear();
        st.window.draw(st.sprite);
        st.window.display();
        time_counter.End();

        if (time_counter.Count() % 10 == 0) {
            std::cout << "Average display time: " << time_counter.GetAvr() <<" ms over " << time_counter.Count() << " frames" << std::endl;
        }
    });
}
}  // namespace render
