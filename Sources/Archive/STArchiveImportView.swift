import SwiftUI
import UniformTypeIdentifiers

/// Standalone import diagnostics screen. Embed in the launcher navigation
/// after verifying the existing screen structure.
struct STArchiveImportView: View {
    @State private var showingImporter = false
    @State private var isImporting = false
    @State private var importStatus: String?
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
                    .disabled(isImporting)
                    .buttonStyle(.borderedProminent)
                if isImporting { ProgressView("Copying game archives…") }
                if let importStatus { Text(importStatus).font(.subheadline) }
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
                selectedFolder = folder.lastPathComponent
                report = nil
                errorMessage = nil
                isImporting = true
                importStatus = "Copying selected archives into application storage…"
                DispatchQueue.global(qos: .userInitiated).async {
                    // STArchiveStore owns security-scoped access for the entire copy.
                    let result = Result { try STArchiveStore.importArchives(from: folder) }
                    DispatchQueue.main.async {
                        isImporting = false
                        switch result {
                        case .success(let imported):
                            report = imported
                            importStatus = "Imported \(imported.archives.count) archive(s). Contents not yet validated."
                            errorMessage = nil
                        case .failure(let error):
                            report = nil
                            importStatus = nil
                            errorMessage = "Import failed: \(error.localizedDescription)"
                        }
                    }
                }
            }
        }
    }
}
