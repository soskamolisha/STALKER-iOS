import SwiftUI
import MetalKit
import simd

/// Minimal wireframe-style triangle preview using a Metal render pipeline.
/// This is a diagnostic viewer, not a game renderer.
struct CollisionMetalPreview: UIViewRepresentable {
    let mesh: CollisionPreviewMesh

    func makeCoordinator() -> Renderer { Renderer(mesh: mesh) }
    func makeUIView(context: Context) -> MTKView {
        let view = MTKView()
        view.device = MTLCreateSystemDefaultDevice()
        view.clearColor = MTLClearColor(red: 0.035, green: 0.06, blue: 0.05, alpha: 1)
        view.colorPixelFormat = .bgra8Unorm
        view.delegate = context.coordinator
        view.isPaused = false
        view.enableSetNeedsDisplay = false
        context.coordinator.configure(view: view)
        return view
    }
    func updateUIView(_ uiView: MTKView, context: Context) {}

    final class Renderer: NSObject, MTKViewDelegate {
        private let mesh: CollisionPreviewMesh
        private var queue: MTLCommandQueue?
        private var pipeline: MTLRenderPipelineState?
        private var vertexBuffer: MTLBuffer?
        private var indexBuffer: MTLBuffer?
        private var indexCount = 0
        private var start = ProcessInfo.processInfo.systemUptime

        init(mesh: CollisionPreviewMesh) { self.mesh = mesh }

        func configure(view: MTKView) {
            guard let device = view.device,
                  let queue = device.makeCommandQueue() else { return }
            self.queue = queue
            let source = """
            #include <metal_stdlib>
            using namespace metal;
            struct Uniforms { float4x4 transform; };
            vertex float4 vs_main(const device packed_float3* positions [[buffer(0)]],
                                  constant Uniforms& uniforms [[buffer(1)]],
                                  uint vid [[vertex_id]]) {
                return uniforms.transform * float4(float3(positions[vid]), 1.0);
            }
            fragment float4 fs_main() { return float4(0.83, 0.73, 0.28, 1.0); }
            """
            guard let library = try? device.makeLibrary(source: source, options: nil),
                  let vertex = library.makeFunction(name: "vs_main"),
                  let fragment = library.makeFunction(name: "fs_main") else { return }
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertex
            descriptor.fragmentFunction = fragment
            descriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
            pipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
            let positions = mesh.vertices.flatMap { [$0.x, $0.y, $0.z] }
            vertexBuffer = positions.withUnsafeBytes {
                device.makeBuffer(bytes: $0.baseAddress!, length: $0.count, options: [])
            }
            indexBuffer = mesh.indices.withUnsafeBytes {
                device.makeBuffer(bytes: $0.baseAddress!, length: $0.count, options: [])
            }
            indexCount = mesh.indices.count
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}
        func draw(in view: MTKView) {
            guard let pipeline, let queue, let vertexBuffer, let indexBuffer,
                  indexCount > 0, let drawable = view.currentDrawable,
                  let pass = view.currentRenderPassDescriptor,
                  let command = queue.makeCommandBuffer(),
                  let encoder = command.makeRenderCommandEncoder(descriptor: pass) else { return }
            let elapsed = Float(ProcessInfo.processInfo.systemUptime - start)
            let aspect = Float(view.drawableSize.width / max(view.drawableSize.height, 1))
            let angle = elapsed * 0.15
            let rotation = simd_float4x4(
                SIMD4<Float>(cos(angle), 0, -sin(angle), 0),
                SIMD4<Float>(0, 1, 0, 0),
                SIMD4<Float>(sin(angle), 0, cos(angle), 0),
                SIMD4<Float>(0, 0, 0, 1))
            // Center large world-coordinate meshes before rotating them.
            let first = mesh.vertices.first ?? SIMD3<Float>(repeating: 0)
            let bounds = mesh.vertices.reduce((first, first)) { partial, point in
                (simd_min(partial.0, point), simd_max(partial.1, point))
            }
            let center = (bounds.0 + bounds.1) * 0.5
            let extent = max(simd_length(bounds.1 - bounds.0), 0.001)
            let scale = Float(1.5) / extent
            var translation = matrix_identity_float4x4
            translation.columns.3 = SIMD4<Float>(-center.x, -center.y, -center.z, 1)
            let projection = simd_float4x4(diagonal: SIMD4<Float>(scale / max(aspect, 0.01), scale, scale, 1))
            var transform = projection * rotation * translation
            encoder.setRenderPipelineState(pipeline)
            encoder.setVertexBuffer(vertexBuffer, offset: 0, index: 0)
            encoder.setVertexBytes(&transform, length: MemoryLayout<simd_float4x4>.stride, index: 1)
            encoder.setTriangleFillMode(.lines)
            encoder.drawIndexedPrimitives(type: .triangle, indexCount: indexCount,
                                          indexType: .uint32, indexBuffer: indexBuffer, indexBufferOffset: 0)
            encoder.endEncoding()
            command.present(drawable)
            command.commit()
        }
    }
}
