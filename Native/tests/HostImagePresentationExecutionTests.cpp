#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "BarriEww/Vulkan/VulkanContext.hpp"
#include "HostImagePresentationExecutionFixture.hpp"
#include "TestVulkanDeviceHarness.hpp"

using barrieww::VulkanContext;
using barrieww::testing::HostImagePresentationExecutionFixture;
using barrieww::testing::TestVulkanDeviceHarness;

namespace {

constexpr std::size_t g_hostImageByteCount =
    HostImagePresentationExecutionFixture::s_hostImageByteCount;

/**
 * @note ThreadSafety: Compile-time-only and safe from every thread.
 * @brief Reports whether configure-time CI policy requires real Vulkan execution.
 * @return bool True when unavailable execution capabilities must fail the test
 * @warning MemoryOwnership: Returns a value and accesses no memory or Vulkan object.
 */
constexpr bool isExecutionEnvironmentRequired() noexcept {
#if defined(BARRIEWW_HOST_IMAGE_PRESENTATION_EXECUTION_ENVIRONMENT_REQUIRED)
    return true;
#else
    return false;
#endif
}

/**
 * @note ThreadSafety: Test-thread confined because FAIL and SKIP alter Catch2 control flow.
 * @brief Accepts an available capability, fails when CI requires it, and skips locally.
 * @param bool isAvailable Whether the required driver and extension profile is available
 * @param const char* unavailableMessage Stable diagnostic for the unavailable capability
 * @warning MemoryOwnership: Borrows unavailableMessage and transfers no ownership.
 */
void requireExecutionEnvironment(bool isAvailable, const char* unavailableMessage) {
    if (isAvailable) {
        return;
    }
#if defined(BARRIEWW_HOST_IMAGE_PRESENTATION_EXECUTION_ENVIRONMENT_REQUIRED)
    FAIL(unavailableMessage);
#else
    SKIP(unavailableMessage);
#endif
}

/**
 * @note ThreadSafety: Creates one harness owned by the calling test thread.
 * @brief Creates the Vulkan 1.2 extension-only harness or applies the test-only forced
 *        unavailable seam used to verify CI failure policy.
 * @return std::unique_ptr<TestVulkanDeviceHarness> Harness owner, or nullptr when unavailable
 * @warning MemoryOwnership: Transfers harness ownership to the caller on success.
 */
std::unique_ptr<TestVulkanDeviceHarness> createExecutionHarness() {
#if defined(BARRIEWW_HOST_IMAGE_PRESENTATION_EXECUTION_ENVIRONMENT_FORCED_UNAVAILABLE)
    return nullptr;
#else
    return TestVulkanDeviceHarness::createWithVulkan12DynamicRenderingExtension();
#endif
}

/**
 * @note ThreadSafety: Test-thread confined during one synchronous queue submission.
 * @brief Records, submits, and reads one fixture execution into tightly packed bytes.
 * @param TestVulkanDeviceHarness& harness Harness providing command allocation and queue
 * @param HostImagePresentationExecutionFixture& fixture Initialized execution fixture
 * @param const std::array<std::byte, g_hostImageByteCount>& hostBytes Source RGBA8 bytes
 * @return std::vector<std::byte> Destination bytes after successful execution
 * @warning MemoryOwnership: Borrows harness and fixture and returns an owned byte vector.
 */
std::vector<std::byte> executeAndReadback(
    TestVulkanDeviceHarness& harness,
    HostImagePresentationExecutionFixture& fixture,
    const std::array<std::byte, g_hostImageByteCount>& hostBytes) {
    const VkCommandBuffer commandBuffer = harness.allocateCommandBuffer();
    REQUIRE(commandBuffer != VK_NULL_HANDLE);
    REQUIRE(fixture.record(commandBuffer, hostBytes));
    REQUIRE(harness.submitAndWait(commandBuffer));
    std::vector<std::byte> readbackBytes;
    REQUIRE(fixture.readback(readbackBytes));
    return readbackBytes;
}

} // namespace

TEST_CASE("Host image presentation execution policy matches configure environment",
          "[hostImagePresentationExecution][policy]") {
#if defined(BARRIEWW_HOST_IMAGE_PRESENTATION_EXECUTION_ENVIRONMENT_REQUIRED)
    STATIC_REQUIRE(isExecutionEnvironmentRequired());
#else
    STATIC_REQUIRE_FALSE(isExecutionEnvironmentRequired());
#endif
}

TEST_CASE("Host image presentation reproduces Minecraft image pixels",
          "[hostImagePresentationExecution][gpu]") {
    auto harness = createExecutionHarness();
    requireExecutionEnvironment(
        harness != nullptr,
        "Vulkan 1.2 graphics driver with VK_KHR_dynamic_rendering is unavailable");
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    HostImagePresentationExecutionFixture fixture{
        vulkanContext,
        VkExtent2D{HostImagePresentationExecutionFixture::s_hostImageWidth,
                   HostImagePresentationExecutionFixture::s_hostImageHeight}};
    REQUIRE(fixture.initialize());

    // Row zero is Minecraft's lower edge but Vulkan's positive-height destination upper
    // edge. Identical byte rows therefore encode the required presentation Y inversion.
    constexpr std::array<std::byte, g_hostImageByteCount> hostBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{17}, std::byte{31}, std::byte{47}, std::byte{251},
        std::byte{67}, std::byte{83}, std::byte{101}, std::byte{239},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{13}, std::byte{29}, std::byte{43}, std::byte{227},
        std::byte{59}, std::byte{71}, std::byte{89}, std::byte{223},
        std::byte{107}, std::byte{127}, std::byte{149}, std::byte{211},
        std::byte{163}, std::byte{181}, std::byte{199}, std::byte{197},
        std::byte{19}, std::byte{37}, std::byte{53}, std::byte{193},
        std::byte{73}, std::byte{97}, std::byte{109}, std::byte{191},
        std::byte{131}, std::byte{151}, std::byte{173}, std::byte{179},
        std::byte{191}, std::byte{211}, std::byte{229}, std::byte{167},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{23}, std::byte{61}, std::byte{113}, std::byte{157},
        std::byte{137}, std::byte{79}, std::byte{41}, std::byte{149},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
    constexpr std::array<std::byte, g_hostImageByteCount> expectedBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{17}, std::byte{31}, std::byte{47}, std::byte{251},
        std::byte{67}, std::byte{83}, std::byte{101}, std::byte{239},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{13}, std::byte{29}, std::byte{43}, std::byte{227},
        std::byte{59}, std::byte{71}, std::byte{89}, std::byte{223},
        std::byte{107}, std::byte{127}, std::byte{149}, std::byte{211},
        std::byte{163}, std::byte{181}, std::byte{199}, std::byte{197},
        std::byte{19}, std::byte{37}, std::byte{53}, std::byte{193},
        std::byte{73}, std::byte{97}, std::byte{109}, std::byte{191},
        std::byte{131}, std::byte{151}, std::byte{173}, std::byte{179},
        std::byte{191}, std::byte{211}, std::byte{229}, std::byte{167},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{23}, std::byte{61}, std::byte{113}, std::byte{157},
        std::byte{137}, std::byte{79}, std::byte{41}, std::byte{149},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};

    const std::vector<std::byte> readbackBytes =
        executeAndReadback(*harness, fixture, hostBytes);
    REQUIRE(readbackBytes.size() == expectedBytes.size());
    REQUIRE(std::equal(readbackBytes.begin(), readbackBytes.end(),
                       expectedBytes.begin(), expectedBytes.end()));
}

TEST_CASE("Host image presentation preserves nearest filtering while scaling",
          "[hostImagePresentationExecution][gpu]") {
    auto harness = createExecutionHarness();
    requireExecutionEnvironment(
        harness != nullptr,
        "Vulkan 1.2 graphics driver with VK_KHR_dynamic_rendering is unavailable");
    const VulkanContext vulkanContext{harness->makeContextCreateInfo()};
    HostImagePresentationExecutionFixture fixture{vulkanContext, VkExtent2D{5u, 4u}};
    REQUIRE(fixture.initialize());

    constexpr std::array<std::byte, g_hostImageByteCount> hostBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
        std::byte{16}, std::byte{32}, std::byte{48}, std::byte{255},
        std::byte{64}, std::byte{80}, std::byte{96}, std::byte{255},
        std::byte{112}, std::byte{128}, std::byte{144}, std::byte{255},
        std::byte{160}, std::byte{176}, std::byte{192}, std::byte{255},
        std::byte{15}, std::byte{45}, std::byte{75}, std::byte{255},
        std::byte{105}, std::byte{135}, std::byte{165}, std::byte{255},
        std::byte{195}, std::byte{225}, std::byte{30}, std::byte{255},
        std::byte{60}, std::byte{90}, std::byte{120}, std::byte{255},
        std::byte{7}, std::byte{53}, std::byte{101}, std::byte{255},
        std::byte{149}, std::byte{197}, std::byte{241}, std::byte{255},
        std::byte{29}, std::byte{83}, std::byte{137}, std::byte{255},
        std::byte{191}, std::byte{223}, std::byte{251}, std::byte{255}};
    constexpr std::array<std::byte, 80u> expectedBytes{
        std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{255}, std::byte{0}, std::byte{255},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255},
        std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255},
        std::byte{16}, std::byte{32}, std::byte{48}, std::byte{255},
        std::byte{64}, std::byte{80}, std::byte{96}, std::byte{255},
        std::byte{112}, std::byte{128}, std::byte{144}, std::byte{255},
        std::byte{112}, std::byte{128}, std::byte{144}, std::byte{255},
        std::byte{160}, std::byte{176}, std::byte{192}, std::byte{255},
        std::byte{15}, std::byte{45}, std::byte{75}, std::byte{255},
        std::byte{105}, std::byte{135}, std::byte{165}, std::byte{255},
        std::byte{195}, std::byte{225}, std::byte{30}, std::byte{255},
        std::byte{195}, std::byte{225}, std::byte{30}, std::byte{255},
        std::byte{60}, std::byte{90}, std::byte{120}, std::byte{255},
        std::byte{7}, std::byte{53}, std::byte{101}, std::byte{255},
        std::byte{149}, std::byte{197}, std::byte{241}, std::byte{255},
        std::byte{29}, std::byte{83}, std::byte{137}, std::byte{255},
        std::byte{29}, std::byte{83}, std::byte{137}, std::byte{255},
        std::byte{191}, std::byte{223}, std::byte{251}, std::byte{255}};

    const std::vector<std::byte> readbackBytes =
        executeAndReadback(*harness, fixture, hostBytes);
    REQUIRE(readbackBytes.size() == expectedBytes.size());
    REQUIRE(std::equal(readbackBytes.begin(), readbackBytes.end(),
                       expectedBytes.begin(), expectedBytes.end()));
}
