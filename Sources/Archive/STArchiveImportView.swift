import SwiftUI
import UniformTypeIdentifiers

/// Standalone import diagnostics screen. Embed in the launcher navigation
/// after verifying the existing screen structure.
struct STArchiveImportView: View {
    @State private var showingImporter = false
    @State private var report: STArchiveInventory.Report?
    @State private var selectedFolder: String?
    @State private var errorMessage: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                Text("Game archive diagnostics").font(.title2.bold())
                Text("Select a folder containing your legally obtained gamedata.db0, gamedata.db1, etc. This checks filenames and sizes only; it does not start X-Ray.")
                    .font(.subheadline)
                Button("Choose game folder") { showingImporter = true }
                    .buttonStyle(.borderedProminent)
                if let selectedFolder {
                    Text("Selected: \(selectedFolder)").font(.caption)
                }
                if let report {
                    Text("Archives found: \(report.archives.count)")
                    Text(report.hasBaseArchive ? "Base archive found (not validated)" : "Base archive missing")
                    ForEach(report.archives, id: \.number) { archive in
                        Text("\(archive.filename) — \(archive.bytes) bytes")
                            .font(.system(.caption, design: .monospaced))
                    }
                    if !report.missingNumbers.isEmpty {
                        Text("Missing indices: \(report.missingNumbers.map(String.init).joined(separator: ", "))")
                    }
                    ForEach(Array(report.warnings.enumerated()), id: \.offset) { _, warning in
                        Text(warning).font(.caption).foregroundStyle(.orange)
                    }
                }
                if let errorMessage {
                    Text(errorMessage).foregroundStyle(.red)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding()
        }
        .fileImporter(isPresented: $showingImporter, allowedContentTypes: [.folder], allowsMultipleSelection: false) { result in
            switch result {
            case .failure(let error):
                errorMessage = error.localizedDescription
            case .success(let urls):
                guard let folder = urls.first else {
                    errorMessage = "No folder selected"
                    return
                }
                let granted = folder.startAccessingSecurityScopedResource()
                defer { if granted { folder.stopAccessingSecurityScopedResource() } }
                do {
                    report = try STArchiveInventory.scan(directory: folder)
                    selectedFolder = folder.lastPathComponent
                    errorMessage = nil
                } catch {
                    report = nil
                    errorMessage = "Cannot read selected folder: \(error.localizedDescription)"
                }
            }
        }
    }
}
