#ifndef VOICEGROUP_CORE_DIAGNOSTIC_HPP
#define VOICEGROUP_CORE_DIAGNOSTIC_HPP

#include <string>

namespace voicegroup
{

struct SourcePosition
{
    // Zero-based line number in the analyzed source text.
    int line = 0;

    // Zero-based character offset within line.
    int character = 0;
};

struct SourceRange
{
    // Inclusive start position for diagnostics and editor features.
    SourcePosition start;

    // Exclusive end position for diagnostics and editor features.
    SourcePosition end;
};

enum class DiagnosticSeverity
{
    Error,
    Warning,
    Information,
    Hint,
};

struct Diagnostic
{
    // Source span the diagnostic should point at.
    SourceRange range;

    // Transport-neutral severity; consumers map this to LSP, logs, or UI.
    DiagnosticSeverity severity = DiagnosticSeverity::Error;

    // Stable machine-readable diagnostic identifier.
    std::string code;

    // Human-readable explanation of the issue.
    std::string message;
};

} // namespace voicegroup

#endif // VOICEGROUP_CORE_DIAGNOSTIC_HPP
