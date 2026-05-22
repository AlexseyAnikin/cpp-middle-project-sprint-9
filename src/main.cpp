#include <chrono>
#include <memory>
#include <print>
#include <thread>
#include <utility>
#include <iostream>

#include <SFML/Graphics.hpp>

#include <exec/any_sender_of.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "sfml_display_sender.hpp"
#include "sfml_events_handler.hpp"
#include "types_sfml.hpp"

using namespace std::chrono_literals;
namespace ex = stdexec;

class WaitForFPS {
public:
    static constexpr float TARGET_FPS = 60.0f;
    static constexpr float FRAME_TIME_MS = 1000.0f / TARGET_FPS;

    explicit WaitForFPS(FrameClock &frame_clock, unsigned int target_fps)
        : frame_clock_(frame_clock), frame_time_(1s / target_fps) {}

    void operator()() {
        auto cur_frame_duration = frame_clock_.GetFrameTime();

        if (cur_frame_duration < frame_time_) {
            std::this_thread::sleep_for(frame_time_ - cur_frame_duration);
        }
        frame_clock_.Reset();
    }

private:
    FrameClock &frame_clock_;
    const std::chrono::milliseconds frame_time_ = 1ms;
};

class MandelbrotApp {
public:
    MandelbrotApp() : compute_pool_{std::max(1u, std::thread::hardware_concurrency())}, sfml_thread_{1} {
        std::cout << "hardware_concurrency: \n" 
                  <<  std::thread::hardware_concurrency() 
                  << std::endl;
    }

    void Run() {
        auto compute_sched = compute_pool_.get_scheduler();
        auto sfml_sched = sfml_thread_.get_scheduler();

        auto initialize =
            ex::on(sfml_sched,
                   ex::just() | ex::then([this]() {
                       state_ = std::make_unique<SfmlState>(  //
                           RenderSettings{.width = 800, .height = 600, .max_iterations = 100, .escape_radius = 2.0});
                   }));
        ex::sync_wait(std::move(initialize));

    auto process_frame =
        ex::just() |

        ex::then([this]() {
            SfmlEventHandler handler{
                state_->window,
                state_->render_settings,
                state_->app_state
            };

            handler.Handle();
            }) |

        ex::continues_on(compute_sched) |

        ex::then([this]() {

            if (state_->app_state.should_exit || !state_->app_state.need_rerender) {
                return &state_->fb;
            }   

            state_->app_state.need_rerender = false;

            FrameBuffer* fb = &state_->fb;

            for (std::uint32_t y = 0; y < fb->height; ++y) {
                for (std::uint32_t x = 0; x < fb->width; ++x) {

                    const auto c = mandelbrot::Pixel2DToComplex(
                        x,
                        y,
                        state_->app_state.viewport,
                        fb->width,
                        fb->height
                    );

                    const auto iterations =
                        mandelbrot::CalculateIterationsForPoint(
                            c,
                            state_->render_settings.max_iterations,
                            state_->render_settings.escape_radius
                        );

                    const auto color =
                        mandelbrot::IterationsToColor(
                            iterations,
                            state_->render_settings.max_iterations
                        );

                    const std::size_t index =
                        (static_cast<std::size_t>(y) * fb->width + x) * 4;

                    fb->rgba[index + 0] = color.r;
                    fb->rgba[index + 1] = color.g;
                    fb->rgba[index + 2] = color.b;
                    fb->rgba[index + 3] = 255;
                }
            }

            return &state_->fb;
        }) |

        ex::continues_on(sfml_sched) |
        
        ex::then([this](FrameBuffer* fb) {
                if (state_->app_state.should_exit) {
                    return fb;
                }

        return fb;
        }) |

        render::MakeSfmlDisplaySender(*state_) |

        ex::then([]{}
            // WaitForFPS
            //     state_->frame_clock,
            //     60
            // }
        );

        auto repeated_pipeline = std::move(process_frame) | ex::then([this] { return state_->app_state.should_exit; }) |
                                 exec::repeat_effect_until();
        ex::sync_wait(std::move(repeated_pipeline));
    }

private:
    std::unique_ptr<SfmlState> state_;

    exec::static_thread_pool compute_pool_;
    exec::static_thread_pool sfml_thread_;
};

int main() {
    std::cout << "=== Mandelbrot Fractal Renderer ===\n";
    std::cout << "Controls:" << std::endl;
    std::cout << "  Left Mouse Button  - Zoom In" << std::endl;
    std::cout << "  Right Mouse Button - Zoom Out" << std::endl;
    std::cout << "  X                  - Toggle Auto Zoom (infinite zoom to 'Seahorse Valley' point)" << std::endl;
    std::cout << "  C                  - Reset to Initial View" << std::endl;
    std::cout << "  Close Window       - Exit\n" << std::endl;

    try {
        MandelbrotApp app;
        app.Run();
    } catch (const std::exception &e) {
        std::cout << "Error: {}" << e.what() << std::endl;
        return 1;
    }
    return 0;
}