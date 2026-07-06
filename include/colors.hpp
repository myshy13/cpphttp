#ifndef MYSHY13_COLORS
#define MYSHY13_COLORS
#include <string>

// ===============================================
// ANSI COLOR AND STYLE DEFINITIONS
// (You should place these in a header file)
// ===============================================

namespace Colors {

  // --- BASE FORMATTING & RESET ---

  /** Resets the color and formatting back to default terminal colors. */
  const std::string NC = "\033[0m";

  // --- ATTRIBUTES (Styles) ---

  /** Makes the text bold or bright. */
  const std::string BOLD = "\033[1m";

  /** Makes the text underlined. */
  const std::string UNDERLINE = "\033[4m";

  // --- FOREGROUND COLORS (The main logging colors) ---

  // Basic Standard Colors (Non-bold versions)
  const std::string RED = "\033[31m";     // Default Red
  const std::string GREEN = "\033[32m";   // Default Green
  const std::string YELLOW = "\033[33m";  // Default Yellow
  const std::string BLUE = "\033[34m";    // Default Blue
  const std::string MAGENTA = "\033[35m"; // Default Magenta (Pink)
  const std::string CYAN = "\033[36m";    // Default Cyan
  const std::string WHITE = "\033[37m";   // Default White/Bright Gray

  // --- COMBINED VARIANTS (Bold Colors) ---

  /** Bright Red */
  const std::string BRIGHT_RED = "\033[1;31m";

  /** Bright Green */
  const std::string BRIGHT_GREEN = "\033[1;32m";

  /** Bright Blue */
  const std::string BRIGHT_BLUE = "\033[1;34m";

  // You can create any combination! (e.g., BOLD + BLUE)
}
#endif