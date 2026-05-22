#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types_core.hpp"

class SfmlEventHandler {
public:

    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;
        AppState &state_;

        static constexpr float ZOOM_INTERVAL_MS = 100.0f;

        template <typename R>
        explicit OperationState(R &&r, 
                                sf::RenderWindow &window, 
                                RenderSettings render_settings, 
                                AppState &state)
            : receiver_{std::forward<R>(r)}, 
              window_{window}, 
              render_settings_{render_settings}, 
              state_{state} {}

        void start() noexcept
        {
            try
            {
                HandleEvents(),
                HandleAutoZoom();

                ex::set_value(std::move(receiver_));
            } 
            catch (...)
            {
                std::set_error(std::move(receiver_), std::current_exception());
            } 
        }

    private:
        void HandleEvents() {
            sf::Event event;

            while (window_.pollEvent(event)) {
                switch (event.type) {
                case sf::Event::Closed:
                state_.should_exit = true;
                break;

                case sf::Event::KeyPressed:
                    HandleKeyPress(event.key);
                    break;

                case sf::Event::MouseButtonPressed:
                    HandleMousePress(event.mouseButton);
                    break;

                case sf::Event::MouseButtonReleased:
                    HandleMouseRelease(event.mouseButton);
                    break;

                default:
                    break;
                }
            }
        }

        void HandleKeyPress(const sf::Event::KeyEvent &key) 
        {
            if(key.code == sf::Keyboard::Escape)
            {
                state_.should_exit = true;
                return;
            }

            if(key.code == sf::Keyboard::C)
            {
                state_.viewport = AppState::INITIAL_VIEWPORT;
                state_.need_rerender = true;
                return;
            }

            if(key.code == sf::Keyboard::X)
            {
                state_.auto_zoom_enabled = !state_.auto_zoom_enabled;
                state_.need_rerender = true;
                state_.zoom_clock.restart();
                return;
            }
        }

        void HandleMousePress(const sf::Event::MouseButtonEvent &mouse) 
        {
            if(mouse.button == sf::Mouse::Left)
            {
                state_.left_mouse_pressed = true;
                ZoomToPoint(mouse.x, mouse.y, true);
            }
            else if (mouse.button == sf::Mouse::Right)
            {
                state_.right_mouse_pressed = true;
                ZoomToPoint(mouse.x, mouse.y, false);
            }

            state_.zoom_clock.restart();
            
        }

        void HandleMouseRelease(const sf::Event::MouseButtonEvent &mouse) {
            if (mouse.button == sf::Mouse::Left) {
                state_.left_mouse_pressed = false;
            } else if (mouse.button == sf::Mouse::Right) {
                state_.right_mouse_pressed = false;
            }
        }

        void HandleAutoZoom() 
        {
            if(state_.zoom_clock.getElapsedTime().asMilliseconds() < ZOOM_INTERVAL_MS)
            {
                return;
            }

            if(state_.auto_zoom_enabled)
            {
                ZoomToComplexPoint(
                    AppState::AUTO_ZOOM_TARGET_X,
                    AppState::AUTO_ZOOM_TARGET_Y,
                    true,
                    0.97
                );
                state_.zoom_clock.restart();
                return;
            }

            const auto mouse_position = sf::Mouse::getPosition(window_);

            if(state_.left_mouse_pressed)
            {
                ZoomToPoint(mouse_position.x, mouse_position.y, true);
                state_.zoom_clock.restart();
            }
            else if (state_.right_mouse_pressed)
            {
                ZoomToPoint(mouse_position.x, mouse_position.y, false);
                state_.zoom_clock.restart();
            }
        }

        void ZoomToPoint(int pixel_x, int pixel_y, bool zoom_in, double factor = 0.8) {
            const double target_x = state_.viewport.x_min +
                                    (static_cast<double>(pixel_x) / render_settings_.width) * state_.viewport.width();
            const double target_y = state_.viewport.y_min +
                                    (static_cast<double>(pixel_y) / render_settings_.height) * state_.viewport.height();

            ZoomToComplexPoint(target_x, target_y, zoom_in, factor);

            // const double zoom_factor = zoom_in ? factor : (1.0 / factor);
            // const double new_width = state_.viewport.width() * zoom_factor;
            // const double new_height = state_.viewport.height() * zoom_factor;
            
            /* Ваш код обновления state_ здесь  */
        }

        void ZoomToComplexPoint(double target_x, double target_y, bool zoom_in, double factor = 0.8)
        {
            const double zoom_factor = zoom_in ? factor : (1.0 / target);

            const double old_width = state_.viewport.width();
            const double old_height = state_.viewport.height();

            const double new_width = old_width * zoom_factor;
            const double new_height = old_height * zoom_factor;

            const double x_ratio = (target_x - state_.viewport.x_min) / old_width;
            const double y_ratio = (target_y - state_.viewport.y_min) / old_height;

            state_.viewport.x_min = target_x - x_ratio * new_width;
            state_.viewport.x_max = state_.viewport.x_min + new_width;

            state_.viewport.y_min = target_y -y_ratio * new_height;
            state_.viewport.y_max = state_.viewport.y_min + new_height;

            state_.need_rerender = true;
        }
    };

    sf::RenderWindow& window_;
    RenderSettings render_settings_;
    AppState& state_;

    SfmlEventHandler(sf::RenderWindow &window, RenderSettings render_settings, AppState &state)
        : window_{window}, 
          render_settings_{render_settings}, 
          state_{state} {}

    template <typename Receiver>
    auto connect(Receiver&& receiver)
    {
        return OperationState<std::decay_t<Receiver>>
        {
            std::forward<Receiver>(receiver),
            window_,
            render_settings_,
            state_
        };
    }

    auto get_completion_signatures() const
    {
        return ex::completion_signatures<
            ex::set_value_t(), 
            ex::set_error_t(std::exception_ptr)
        >{};
    }

};
