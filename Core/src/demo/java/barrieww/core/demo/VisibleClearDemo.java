package barrieww.core.demo;

import static org.lwjgl.glfw.GLFW.GLFW_CLIENT_API;
import static org.lwjgl.glfw.GLFW.GLFW_NO_API;
import static org.lwjgl.glfw.GLFW.GLFW_TRUE;
import static org.lwjgl.glfw.GLFW.GLFW_VISIBLE;
import static org.lwjgl.glfw.GLFW.glfwCreateWindow;
import static org.lwjgl.glfw.GLFW.glfwDefaultWindowHints;
import static org.lwjgl.glfw.GLFW.glfwDestroyWindow;
import static org.lwjgl.glfw.GLFW.glfwGetFramebufferSize;
import static org.lwjgl.glfw.GLFW.glfwInit;
import static org.lwjgl.glfw.GLFW.glfwPollEvents;
import static org.lwjgl.glfw.GLFW.glfwSetWindowShouldClose;
import static org.lwjgl.glfw.GLFW.glfwTerminate;
import static org.lwjgl.glfw.GLFW.glfwWaitEvents;
import static org.lwjgl.glfw.GLFW.glfwWindowHint;
import static org.lwjgl.glfw.GLFW.glfwWindowShouldClose;
import static org.lwjgl.glfw.GLFWVulkan.glfwVulkanSupported;
import static org.lwjgl.system.MemoryStack.stackPush;
import static org.lwjgl.system.MemoryUtil.NULL;

import barrieww.core.interoperability.NativePresentationRuntime;
import barrieww.core.interoperability.NativePresentationRuntimeException;
import barrieww.core.interoperability.PresentationBootstrapHandles;
import barrieww.core.interoperability.PresentationClearFrame;
import barrieww.core.interoperability.PresentationFrameMetrics;
import barrieww.core.interoperability.PresentationFrameStatus;
import java.nio.IntBuffer;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.lwjgl.system.MemoryStack;

/**
 * @note ThreadSafety: Single-threaded; runs entirely on the main thread.
 * The baseline cross-platform visible demo (ADR-0004 D5.1): a GLFW window whose swapchain
 * image is cleared to a smoothly animated color every frame through the Native presentation
 * runtime, printing completed-frame CPU metrics (FPS, median/p95/p99). Requires a display, so
 * it is run manually via the {@code runVisibleDemo} Gradle task, not in headless CI.
 */
public final class VisibleClearDemo {
    private static final int s_framesInFlight = 2;

    private VisibleClearDemo() {
    }

    /**
     * Runs the visible clear demo until the window is closed.
     *
     * @param String[] arguments Unused command-line arguments
     */
    public static void main(String[] arguments) {
        String nativeLibraryPathProperty = System.getProperty("barrieww.nativeLibraryPath");
        if (nativeLibraryPathProperty == null) {
            throw new IllegalStateException(
                    "Set -Dbarrieww.nativeLibraryPath=<absolute BarriEwwNativeFfm path>");
        }
        Path nativeLibraryPath = Path.of(nativeLibraryPathProperty).toAbsolutePath();

        if (!glfwInit()) {
            throw new IllegalStateException("Failed to initialize GLFW");
        }
        try {
            if (!glfwVulkanSupported()) {
                throw new IllegalStateException("GLFW reports no Vulkan loader");
            }
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
            long window = glfwCreateWindow(1280, 720, "Barri-Eww Visible Clear Demo", NULL,
                    NULL);
            if (window == NULL) {
                throw new IllegalStateException("Failed to create the GLFW window");
            }
            try (VulkanBootstrap bootstrap = VulkanBootstrap.create(window)) {
                runRenderLoop(window, nativeLibraryPath, bootstrap.handles());
            } finally {
                glfwDestroyWindow(window);
            }
        } finally {
            glfwTerminate();
        }
    }

    private static void runRenderLoop(long window, Path nativeLibraryPath,
                                      PresentationBootstrapHandles handles) {
        NativePresentationRuntime runtime = null;
        NativePresentationRuntimeException primaryFrameFailure = null;
        try {
            List<Long> completedFrameNanoseconds = new ArrayList<>();
            long lastReportNanoseconds = System.nanoTime();
            long frameIndex = 0;

            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                int[] framebufferExtent = queryFramebufferExtent(window);
                int framebufferWidth = framebufferExtent[0];
                int framebufferHeight = framebufferExtent[1];
                if (framebufferWidth == 0 || framebufferHeight == 0) {
                    glfwWaitEvents();
                    continue;
                }
                if (runtime == null) {
                    runtime = openRuntime(nativeLibraryPath, handles, framebufferWidth,
                            framebufferHeight);
                }

                double timeSeconds = frameIndex / 240.0;
                float clearRed = (float) (0.5 + 0.5 * Math.sin(timeSeconds));
                float clearGreen = (float) (0.5 + 0.5 * Math.sin(timeSeconds + 2.094));
                float clearBlue = (float) (0.5 + 0.5 * Math.sin(timeSeconds + 4.188));

                PresentationClearFrame clearFrame = runtime.presentClearFrame(
                        framebufferWidth, framebufferHeight, clearRed, clearGreen, clearBlue);
                clearFrame.priorMetrics().ifPresent(metrics -> {
                    if (metrics.cpuMetricsValid()) {
                        completedFrameNanoseconds.add(metrics.totalCpuFrameNanoseconds());
                    }
                });
                if (clearFrame.beginStatus() == PresentationFrameStatus.RECREATE_REQUIRED
                        || clearFrame.submitStatus()
                                == PresentationFrameStatus.RECREATE_REQUIRED) {
                    runtime.close();
                    runtime = null;
                }
                ++frameIndex;

                long nowNanoseconds = System.nanoTime();
                if (nowNanoseconds - lastReportNanoseconds >= 1_000_000_000L
                        && !completedFrameNanoseconds.isEmpty()) {
                    reportMetrics(completedFrameNanoseconds,
                            nowNanoseconds - lastReportNanoseconds);
                    completedFrameNanoseconds.clear();
                    lastReportNanoseconds = nowNanoseconds;
                }
            }
        } catch (NativePresentationRuntimeException runtimeException) {
            primaryFrameFailure = runtimeException;
            throw new IllegalStateException("Visible demo frame failed", runtimeException);
        } finally {
            if (runtime != null) {
                try {
                    runtime.close();
                } catch (NativePresentationRuntimeException closeFailure) {
                    if (primaryFrameFailure != null) {
                        primaryFrameFailure.addSuppressed(closeFailure);
                    } else {
                        throw new IllegalStateException(
                                "Failed to close the presentation runtime", closeFailure);
                    }
                }
            }
        }
    }

    private static NativePresentationRuntime openRuntime(Path nativeLibraryPath,
                                                         PresentationBootstrapHandles handles,
                                                         int framebufferWidth,
                                                         int framebufferHeight) {
        try {
            return NativePresentationRuntime.create(nativeLibraryPath, handles,
                    framebufferWidth, framebufferHeight, s_framesInFlight);
        } catch (Exception creationFailure) {
            throw new IllegalStateException("Failed to create the presentation runtime",
                    creationFailure);
        }
    }

    private static int[] queryFramebufferExtent(long window) {
        try (MemoryStack stack = stackPush()) {
            IntBuffer width = stack.mallocInt(1);
            IntBuffer height = stack.mallocInt(1);
            glfwGetFramebufferSize(window, width, height);
            return new int[] {width.get(0), height.get(0)};
        }
    }

    private static void reportMetrics(List<Long> completedFrameNanoseconds,
                                      long intervalNanoseconds) {
        List<Long> sortedNanoseconds = new ArrayList<>(completedFrameNanoseconds);
        Collections.sort(sortedNanoseconds);
        int sampleCount = sortedNanoseconds.size();
        double framesPerSecond = sampleCount * 1_000_000_000.0 / intervalNanoseconds;
        double medianMilliseconds = percentileMilliseconds(sortedNanoseconds, 0.50);
        double p95Milliseconds = percentileMilliseconds(sortedNanoseconds, 0.95);
        double p99Milliseconds = percentileMilliseconds(sortedNanoseconds, 0.99);
        System.out.printf(
                "frames=%d  fps=%.1f  cpu-frame ms median=%.3f p95=%.3f p99=%.3f%n",
                sampleCount, framesPerSecond, medianMilliseconds, p95Milliseconds,
                p99Milliseconds);
    }

    private static double percentileMilliseconds(List<Long> sortedNanoseconds,
                                                 double percentile) {
        int index = (int) Math.floor(percentile * (sortedNanoseconds.size() - 1));
        return sortedNanoseconds.get(index) / 1_000_000.0;
    }

    /** Requests the window to close (used by tests or external drivers). */
    public static void requestClose(long window) {
        glfwSetWindowShouldClose(window, true);
    }
}
