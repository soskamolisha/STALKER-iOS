import SwiftUI

/// Opt-in integration point for the existing launcher screen.
/// Apply `.xrayDiagnosticsAccess()` to the root launcher view.
/// This only opens the read-only developer diagnostic; it does not start X-Ray.
private struct XRayDiagnosticsAccessModifier: ViewModifier {
    @State private var showingDiagnostics = false

    func body(content: Content) -> some View {
        content
            .safeAreaInset(edge: .bottom, spacing: 0) {
                HStack {
                    Spacer()
                    Button {
                        showingDiagnostics = true
                    } label: {
                        Label("File diagnostics", systemImage: "doc.text.magnifyingglass")
                    }
                    .buttonStyle(.bordered)
                    .padding(8)
                    .accessibilityHint("Inspect a small binary file without importing game assets")
                }
                .background(.ultraThinMaterial)
            }
            .sheet(isPresented: $showingDiagnostics) {
                XRayChunkDiagnosticView()
            }
    }
}

extension View {
    func xrayDiagnosticsAccess() -> some View {
        modifier(XRayDiagnosticsAccessModifier())
    }
}
