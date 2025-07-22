// enhanced_html_parser.h
//
// Enhanced HTML parser for beautiful terminal email display
// Provides rich text formatting, emoji support, and layout preservation

#pragma once

#include <string>
#include <vector>
#include <map>
#include <regex>

struct HtmlElement
{
  std::string tag;
  std::map<std::string, std::string> attributes;
  std::string content;
  std::vector<HtmlElement> children;
  int colorPair = -1;
  bool isBold = false;
  bool isItalic = false;
  bool isUnderlined = false;
};

struct TerminalFormat
{
  std::string text;
  int colorPair = -1;
  bool isBold = false;
  bool isItalic = false;
  bool isUnderlined = false;
  bool isLink = false;
  std::string linkUrl;
  int indentLevel = 0;
};

class HtmlParser
{
public:
  HtmlParser();
  ~HtmlParser();

  // Main parsing function
  std::vector<TerminalFormat> ParseHtmlToTerminal(const std::string& htmlContent);
  
  // Configuration
  void SetTerminalWidth(int width) { m_terminalWidth = width; }
  void SetColorScheme(const std::map<std::string, int>& colors) { m_colorScheme = colors; }
  void EnableEmojiConversion(bool enable) { m_convertEmojis = enable; }
  void EnableLinkExtraction(bool enable) { m_extractLinks = enable; }
  void EnableTableRendering(bool enable) { m_renderTables = enable; }
  void SetIndentSize(int size) { m_indentSize = size; }

private:
  // HTML parsing helpers
  HtmlElement ParseElement(const std::string& html, size_t& position);
  std::map<std::string, std::string> ParseAttributes(const std::string& attrString);
  std::string ExtractTagName(const std::string& tagString);
  
  // Terminal formatting
  std::vector<TerminalFormat> ConvertElementToTerminal(const HtmlElement& element, int depth = 0);
  std::string ProcessTextContent(const std::string& text, const HtmlElement& parent);
  
  // Text processing
  std::string ConvertHtmlEntities(const std::string& text);
  std::string ConvertEmojis(const std::string& text);
  std::string WrapText(const std::string& text, int maxWidth, int indent = 0);
  std::string ExtractLinks(const std::string& text, std::vector<std::string>& links);
  
  // Table processing
  std::vector<TerminalFormat> RenderTable(const HtmlElement& table);
  std::vector<std::vector<std::string>> ExtractTableData(const HtmlElement& table);
  std::string FormatTableRow(const std::vector<std::string>& row, 
                            const std::vector<int>& columnWidths);
  
  // List processing
  std::vector<TerminalFormat> RenderList(const HtmlElement& list, bool isOrdered = false);
  
  // Color and formatting
  int GetColorForElement(const std::string& tag);
  bool ShouldBeBold(const std::string& tag);
  bool ShouldBeItalic(const std::string& tag);
  bool ShouldBeUnderlined(const std::string& tag);
  
  // Utility functions
  std::string Trim(const std::string& str);
  std::string ToLower(const std::string& str);
  bool IsWhitespace(char c);
  
private:
  int m_terminalWidth = 80;
  int m_indentSize = 2;
  bool m_convertEmojis = true;
  bool m_extractLinks = true;
  bool m_renderTables = true;
  
  std::map<std::string, int> m_colorScheme;
  std::vector<std::string> m_extractedLinks;
  
  // HTML entity map
  static const std::map<std::string, std::string> s_htmlEntities;
  
  // Emoji conversion map
  static const std::map<std::string, std::string> s_emojiMap;
  
  // Tag formatting rules
  static const std::map<std::string, bool> s_boldTags;
  static const std::map<std::string, bool> s_italicTags;
  static const std::map<std::string, bool> s_underlineTags;
  static const std::map<std::string, int> s_defaultColors;
};

// Utility class for terminal color management
class TerminalColors
{
public:
  static void InitializeColors();
  static int CreateColorPair(int fg, int bg);
  static int GetColorCode(const std::string& colorName);
  static void SetupEnhancedPalette();
  
private:
  static std::map<std::string, int> s_colorMap;
  static int s_nextColorPair;
};
