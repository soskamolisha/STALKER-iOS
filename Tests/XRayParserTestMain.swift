import Foundation

@main
struct XRayParserTestMain {
    static func main() {
        let result = XRayParserSelfTest.run()
        print("X-Ray parser checks passed: \(result.passed)")
        for failure in result.failed {
            fputs("FAIL: \(failure)\n", stderr)
        }
        if !result.isSuccessful { exit(1) }
    }
}
