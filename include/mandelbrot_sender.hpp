#pragma once

#include "mandelbrot_fractal_utils.hpp"
#include "types_sfml.hpp"

#include <stdexec/execution.hpp>
#include "types_core.hpp"

#include <cmath>
#include <exception>
#include <vector>



using namespace std::chrono_literals;
namespace ex = stdexec;

namespace mandelbrot {

inline sf::Color ComputeColor(int iterations, int max_iterations)
{
    if(iterations >= max_iterations)
    {
        return sf::Color::Black;
    }

    const double t = static_cast<double>(iterations) / max_iterations;

    const std::uint8_t r = static_cast<std::uint8_t>(9 * (1 - t) * t * t * t * 255);
    const std::uint8_t g = static_cast<std::uint8_t>(15 * (1 - t) * (1 - t) * t * t * 256);
    const std::uint8_t b = static_cast<std::uint8_t>(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 256);

    return sf::Color(r, g, b);
}

inline int ComputePoint(double cx, double cy, int max_iterations, double escape_radius)
{
    double x = 0.0;
    double y = 0.0;

    int iteration = 0;

    while((x * x + y * y <= escape_radius * escape_radius) && iteration < max_iterations)
    {
        const double xtemp = x * x - y * y + cx;

        y = 2.0 * x * y + cy;
        x = xtemp;

        ++iteration;
    }

    return iteration;
}

class ComputeSender 
{
public:
    template <typename Receiver>
    struct OperationState
    {
        Receiver receiver_;
        RenderSettings settings_;
        ViewPort viewport_;

        std::vector<std::uint8_t> pixels_;

        template <typename R>
        OperationState(
            R&& receiver,
            RenderSettings settings,
            ViewPort viewport) : receiver_(std::forward<R>(receiver)),
                                 settings_(settings),
                                 viewport_(viewport) {} 

        void start() noexcept 
        {
            try
            {
                Compute();

                ex::set_value(
                    std::move(receiver_),
                    std::move(pixels_)
                );
            } catch (...) {
                ex::set_error(
                    std::move(receiver_),
                    std::current_exception()
                );
            }
        }

        private:
            void Compute()
            {
                pixels_.resize(settings_.width * settings_.height * 4);

                for(unsigned int py = 0; py < settings_.height; ++py)
                {
                    for(unsigned int px = 0; px < settings_.width; ++px)
                    {
                        const double x0 = viewport_.x_min + 
                                          (static_cast<double>(px) / settings_.width) * 
                                          viewport_.width();

                        const double y0 = viewport_.y_min +
                                          (static_cast<double>(py) / settings_.height) *
                                          viewport_.height();

                        const int iterations = ComputePoint(
                            x0,
                            y0,
                            settings_.max_iterations,
                            settings_.escape_radius
                        );

                        const sf::Color color = ComputeColor(iterations, settings_.max_iterations);

                        const std::size_t index = (py * settings_.width + px) * 4;

                        pixels_[index + 0] = color.r;
                        pixels_[index + 1] = color.g;
                        pixels_[index + 2] = color.b;
                        pixels_[index + 3] = 255;
                    }
                }
            }
    };

    ComputeSender(RenderSettings settings, ViewPort viewport)
        : settings_(settings), viewport_(viewport) {}

    template <typename Receiver>
    auto connect(Receiver&& receiver)
    {
        return OperationState<std::decay_t<Receiver>>
        {
            std::forward<Receiver>(receiver),
            settings_,
            viewport_
        };
    }

    auto get_completion_signatures() const
    {
        return ex::completion_signatures<
            ex::set_value_t(std::vector<std::uint8_t>),
            ex::set_error_t(std::exception_ptr)
        >{};
    }

private:
    RenderSettings settings_;
    ViewPort viewport_;
};

inline auto MakeComputeSender(RenderSettings settings, ViewPort viewport)
{
    return ComputeSender
    {
        settings,
        viewport
    };
}

}  // namespace mandelbrot
