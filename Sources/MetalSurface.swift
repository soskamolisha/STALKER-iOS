import SwiftUI
import MetalKit

/// Minimal Metal rendering surface. This clears a drawable; it does NOT render X-Ray scenes.
struct MetalSurface: UIViewRepresentable {
    func makeCoordinator() -> Renderer { Renderer() }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.025, green: 0.04, blue: 0.03, alpha: 1)
        view.delegate = context.coordinator
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {}

    final class Renderer: NSObject, MTKViewDelegate {
        private let commandQueue = MTLCreateSystemDefaultDevice()?.makeCommandQueue()

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard let pass = view.currentRenderPassDescriptor,
                  let drawable = view.currentDrawable,
                  let queue = commandQueue,
                  let buffer = queue.makeCommandBuffer(),
                  let encoder = buffer.makeRenderCommandEncoder(descriptor: pass)
            else { return }
            encoder.endEncoding()
            buffer.present(drawable)
            buffer.commit()
        }
    }
}
