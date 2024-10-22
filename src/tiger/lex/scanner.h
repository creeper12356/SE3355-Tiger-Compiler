#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <cstdarg>
#include <iostream>
#include <string>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

class Scanner : public ScannerBase {
public:
  Scanner() = delete;
  explicit Scanner(std::string_view fname, std::ostream &out = std::cout)
      : ScannerBase(std::cin, out), comment_level_(1), char_pos_(1),
        errormsg_(std::make_unique<err::ErrorMsg>(fname)) {
    switchStreams(errormsg_->infile_, out);
  }

  /**
   * Output an error
   * @param message error message
   */
  void Error(int pos, std::string message, ...) {
    va_list ap;
    va_start(ap, message);
    errormsg_->Error(pos, message, ap);
    va_end(ap);
  }

  /**
   * Getter for `tok_pos_`
   */
  [[nodiscard]] int GetTokPos() const { return errormsg_->GetTokPos(); }

  /**
   * Transfer the ownership of `errormsg_` to the outer scope
   * @return unique pointer to errormsg
   */
  [[nodiscard]] std::unique_ptr<err::ErrorMsg> TransferErrormsg() {
    return std::move(errormsg_);
  }

  int lex();

private:
  int comment_level_;
  std::string string_buf_;
  int char_pos_;
  std::unique_ptr<err::ErrorMsg> errormsg_;

  /**
   * NOTE: do not change all the funtion signature below, which is used by
   * flexc++ internally
   */
  int lex_();
  int executeAction_(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum_ type);
  void adjust();
  void adjustStr();
};

inline int Scanner::lex() { return lex_(); }

inline void Scanner::preCode() {
  // Optionally replace by your own code
}

inline void Scanner::postCode(PostEnum_ type) {
  // Optionally replace by your own code
}

inline std::string handleEscape(const std::string &source) {
  // std::cout << "source: " << source << std::endl;
  assert(source.size() >= 2);
  auto str_content_len = source.size() - 2;
  bool escaping = false;
  std::string result_str;

  for(auto i = 1;i <= str_content_len; ++i) {
    if(!escaping) {
      if(source[i] != '\\') {
        // 不进行转义
        result_str.push_back(source[i]);
      } else {
        // 开始转义
        escaping = true;
      }

      continue;
    }

    // 处理转义
    if(source[i] == 't') {
      result_str.push_back('\t');
    } else if(source[i] == 'n') {
      result_str.push_back('\n');
    } else if(source[i] == '"') {
      result_str.push_back('"');
    } else if(source[i] == '\\') {
      result_str.push_back('\\');
    } 
      else if(isdigit(source[i])) {
      assert(i + 1 <= str_content_len);
      assert(i + 2 <= str_content_len);
      assert(isdigit(source[i + 1]));
      assert(isdigit(source[i + 2]));
      unsigned char ascii_code = (source[i] - '0') * 100 + (source[i + 1] - '0') * 10 + (source[i + 2] - '0');
      result_str.push_back(ascii_code);
      i += 2;
    } else if(isspace(source[i])) {

      while(true) {
        ++i;
        if(source[i] == '\\') {
          break;
        } else {
          assert(isspace(source[i]));
        }
      }
    } else if(source[i] == '^') {
      assert(i + 1 <= str_content_len);
      result_str.push_back(source[i + 1] - 64);
      ++i;
    } else {
      std::cout << "source: " << source << std::endl;
      std::cout << "unimplemented: " << int(source[i]) << std::endl;
      assert(false);
    }

    escaping = false;
  }
  return result_str;
}

inline void Scanner::print() { print_(); }

inline void Scanner::adjust() {
  errormsg_->tok_pos_ = char_pos_;
  char_pos_ += length();
}

inline void Scanner::adjustStr() { char_pos_ += length(); }

#endif // TIGER_LEX_SCANNER_H_
