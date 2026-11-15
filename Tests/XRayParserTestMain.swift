import XCTest
@testable import STALKERiOSStarter

/// Runs the parser's synthetic-data checks inside the iOS XCTest bundle.
final class XRayParserSelfTests: XCTestCase {
    func testParserSelfChecks() {
        let result = XRayParserSelfTest.run()
        XCTAssertTrue(result.isSuccessful, "X-Ray parser failures: \(result.failed.joined(separator: ", "))")
        XCTAssertGreaterThan(result.passed, 0)
    }
}
