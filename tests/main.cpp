#include <gtest/gtest.h>

#include <stdexec/execution.hpp>

#include <exception>
#include <vector>
#include <cstdint>
#include <algorithm>

#include "mandelbrot_fractal_utils.hpp"
#include "mandelbrot_sender.hpp"
#include "types_core.hpp"
#include "types_sfml.hpp"

namespace ex = stdexec;

struct ComputeTestReceiver
{
    bool* value_called{};
    bool* error_called{};
    std::vector<std::uint8_t>* pixels{};

    friend void tag_invoke(ex::set_value_t, ComputeTestReceiver&& receiver, 
                           std::vector<std::uint8_t> result) noexcept
                           {
                            *receiver.value_called = true;
                            *receiver.pixels = std::move(result);
                           }

    friend void tag_invoke(ex::set_error_t, ComputeTestReceiver&& receiver,
                           std::exception_ptr) noexcept
                           {
                            *receiver.error_called = true;
                           }

    friend void tag_invoke(ex::set_stopped_t, ComputeTestReceiver&&) noexcept {}

};




TEST(MandelbrotUtilsTest, PointInsideSetReachesMaxIterations) 
{
    const auto iterations = mandelbrot::CalculateIterationsForPoint(
        mandelbrot::Complex{0.0, 0.0},
        100,
        2.0
    );

    EXPECT_EQ(iterations, 100);
}

TEST(MandelbrotUtilsTest, PointOutsideSetEscapesBeforeMaxIterations) 
{
    const auto iterations = mandelbrot::CalculateIterationsForPoint(
        mandelbrot::Complex{2.0, 2.0},
        100,
        2.0
    );

    EXPECT_LT(iterations, 100);
}

TEST(MandelbrotUtilsTest, PixelToComplexMapsCorners) 
{
    const ViewPort viewport{-2.0, 2.0, -1.0, 1.0};

    const auto top_left = mandelbrot::Pixel2DToComplex(
        0,
        0,
        viewport,
        4,
        2
    );

    EXPECT_DOUBLE_EQ(top_left.real(), -2.0);
    EXPECT_DOUBLE_EQ(top_left.imag(), -1.0);
}

TEST(MandelbrotUtilsTest, MaxIterationsColorIsBlack) 
{
    const auto color = mandelbrot::IterationsToColor(100, 100);

    EXPECT_EQ(color.r, 0);
    EXPECT_EQ(color.g, 0);
    EXPECT_EQ(color.b, 0);
}

TEST(FrameBufferTest, MakeCreatesCorrectRgbaBufferSize) 
{
    const auto fb = FrameBuffer::Make(10, 20);

    EXPECT_EQ(fb.width, 10);
    EXPECT_EQ(fb.height, 20);
    EXPECT_EQ(fb.rgba.size(), 10 * 20 * 4);
}

TEST(ComputeSenderTest, CallsCustomReceiverSetValue) 
{
    RenderSettings settings;
    settings.width = 16;
    settings.height = 12;
    settings.max_iterations = 50;
    settings.escape_radius = 2.0;

    ViewPort viewport = AppState::INITIAL_VIEWPORT;

    bool value_called = false;
    bool error_called = false;
    std::vector<std::uint8_t> pixels;

    auto sender = mandelbrot::MakeComputeSender(settings, viewport);

    auto operation = sender.connect(
        ComputeTestReceiver{
            &value_called,
            &error_called,
            &pixels
        }
    );

    operation.start();

    EXPECT_TRUE(value_called);
    EXPECT_FALSE(error_called);
    EXPECT_EQ(pixels.size(), settings.width * settings.height * 4);
}

TEST(ComputeSenderTest, ProducesOpaquePixels) 
{
    RenderSettings settings;
    settings.width = 8;
    settings.height = 8;
    settings.max_iterations = 50;
    settings.escape_radius = 2.0;

    bool value_called = false;
    bool error_called = false;
    std::vector<std::uint8_t> pixels;

    auto sender = mandelbrot::MakeComputeSender(
        settings,
        AppState::INITIAL_VIEWPORT
    );

    auto operation = sender.connect(
        ComputeTestReceiver{
            &value_called,
            &error_called,
            &pixels
        }
    );

    operation.start();

    ASSERT_TRUE(value_called);
    ASSERT_FALSE(pixels.empty());

    for (std::size_t i = 3; i < pixels.size(); i += 4) {
        EXPECT_EQ(pixels[i], 255);
    }
}

TEST(IntegrationTest, StdexecPipelineComputesNonEmptyImage) 
{
    RenderSettings settings;
    settings.width = 16;
    settings.height = 16;
    settings.max_iterations = 50;
    settings.escape_radius = 2.0;

    ViewPort viewport = AppState::INITIAL_VIEWPORT;

    auto pipeline =
        ex::just() |
        ex::then([settings, viewport] {
            bool value_called = false;
            bool error_called = false;
            std::vector<std::uint8_t> pixels;

            auto sender = mandelbrot::MakeComputeSender(settings, viewport);

            auto operation = sender.connect(
                ComputeTestReceiver{
                    &value_called,
                    &error_called,
                    &pixels
                }
            );

            operation.start();

            EXPECT_TRUE(value_called);
            EXPECT_FALSE(error_called);

            return pixels;
        }) |
        ex::then([](std::vector<std::uint8_t> pixels) {
            const bool has_non_black_pixel =
                std::any_of(
                    pixels.begin(),
                    pixels.end(),
                    [](std::uint8_t value) {
                        return value != 0;
                    }
                );

            EXPECT_TRUE(has_non_black_pixel);
            return pixels.size();
        });

    auto result = ex::sync_wait(std::move(pipeline));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(*result), settings.width * settings.height * 4);
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
