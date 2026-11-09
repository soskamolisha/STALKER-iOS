import SwiftUI
import MetalKit

/// Metal proof of concept: renders a GPU triangle, not X-Ray game content.
struct MetalSurface: UIViewRepresentable {
    func makeCoordinator() -> Renderer { Renderer() }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.clearColor = MTLClearColor(red: 0.025, green: 0.04, blue: 0.03, alpha: 1)
        view.delegate = context.coordinator
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        context.coordinator.configure(for: view)
        return view
    }

    func updateUIView(_ uiView: MTKView, context: Context) {}

    final class Renderer: NSObject, MTKViewDelegate {
        private var queue: MTLCommandQueue?
        private var pipeline: MTLRenderPipelineState?

        func configure(for view: MTKView) {
            guard let device = view.device else { return }
            queue = device.makeCommandQueue()
            let shader = """
            #include <metal_stdlib>
            using namespace metal;
            struct Out { float4 position [[position]]; float4 color; };
            vertex Out vertexMain(uint id [[vertex_id]]) {
                float2 positions[3] = {float2(0.0, 0.5), float2(-0.5, -0.5), float2(0.5, -0.5)};
                Out out;
                out.position = float4(positions[id], 0.0, 1.0);
                out.color = float4(0.9, 0.55, 0.12, 1.0);
                return out;
            }
            fragment float4 fragmentMain(Out in [[stage_in]]) { return in.color; }
            """
            do {
                let library = try device.makeLibrary(source: shader, options: nil)
                let descriptor = MTLRenderPipelineDescriptor()
                descriptor.vertexFunction = library.makeFunction(name: "vertexMain")
                descriptor.fragmentFunction = library.makeFunction(name: "fragmentMain")
                descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
                pipeline = try device.makeRenderPipelineState(descriptor: descriptor)
            } catch {
                NSLog("Metal starter pipeline failed: %@", error.localizedDescription)
            }
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard let pass = view.currentRenderPassDescriptor,
                  let drawable = view.currentDrawable,
                  let queue = queue,
                  let buffer = queue.makeCommandBuffer(),
                  let encoder = buffer.makeRenderCommandEncoder(descriptor: pass)
            else { return }
            if let pipeline = pipeline {
                encoder.setRenderPipelineState(pipeline)
                encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 3)
            }
            encoder.endEncoding()
            buffer.present(drawable)
            buffer.commit()
        }
    }
}
