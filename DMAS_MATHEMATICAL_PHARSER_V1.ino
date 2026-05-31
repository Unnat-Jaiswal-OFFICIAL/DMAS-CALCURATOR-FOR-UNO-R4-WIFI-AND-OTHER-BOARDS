/*
Copyright © 2026 [Unnat Jaiswal]. All rights reserved. No part of this project
may be copied, reproduced, or distributed without explicit permission.
*/

// --- CONFIGURATION CONSTANTS ---
#define MAX_TOKENS 64
#define BAUD_RATE 9600

// Token Type Enumerations
enum TokenType { TOKEN_NUMBER, TOKEN_OP, TOKEN_OPEN_BRACKET, TOKEN_CLOSE_BRACKET, TOKEN_END };

// Structured Token Definition
struct Token {
  TokenType type;
  float value;   // Used if numeric
  char symbol;   // Used for operators/brackets
};

// Global variables for handling input streams
String inputBuffer = "";

void setup() {
  Serial.begin(BAUD_RATE);
  while (!Serial); // Wait for connection

  Serial.println(F("================================================="));
  Serial.println(F("    ARDUINO DMAS MATHEMATICAL PARSING ENGINE     "));
  Serial.println(F("================================================="));
  Serial.println(F("INSTRUCTIONS:"));
  Serial.println(F("1. Input complete equations and press Enter."));
  Serial.println(F("2. Priority Rules: DMAS applies globally."));
  Serial.println(F("3. Bracket Rules: Strictly nested [ { ( math ) } ]"));
  Serial.println(F("Examples: 9 + 9 - 0  OR  4 x 4 / [9 + {8 + (2 * 3)} - 1]"));
  Serial.println(F("=================================================\n"));
}

void loop() {
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    
    // Check for newline string termination
    if (inChar == '\n' || inChar == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        Serial.print(F("Input Equation: "));
        Serial.println(inputBuffer);
        
        // Execute the processing pipeline
        processEquation(inputBuffer);
        
        Serial.println(F("-------------------------------------------------"));
        inputBuffer = ""; // Reset memory pipeline
      }
    } else {
      inputBuffer += inChar;
    }
  }
}

// Master Pipeline Wrapper
void processEquation(String expr) {
  // Step 1: Pre-sanitize and check formatting syntax
  if (!preSanitizeAndValidate(expr)) return;

  Token tokens[MAX_TOKENS];
  int tokenCount = 0;

  // Step 2: Tokenize the raw string into structural objects
  if (!tokenize(expr, tokens, tokenCount)) return;

  // Step 3: Evaluate token stream using recursive DMAS parser
  int tokenIndex = 0;
  bool evaluationSuccess = true;
  float result = parseExpression(tokens, tokenIndex, evaluationSuccess);

  // Step 4: Final verification and output print
  if (evaluationSuccess) {
    if (tokens[tokenIndex].type != TOKEN_END) {
      Serial.println(F("[FATAL ERROR]: Syntax anomaly. Trailing unparsed operators found."));
    } else {
      Serial.print(F("Mathematical Result: "));
      // Print float cleanly without unnecessary trailing zero decimals
      if (result == (long)result) {
        Serial.println((long)result);
      } else {
        Serial.println(result, 6);
      }
    }
  }
}

// Validation Routine (Spaces, Illegal characters, Structural Order)
bool preSanitizeAndValidate(String &expr) {
  // Standardize multiplication signs and drop formatting spaces
  expr.replace(" ", "");
  expr.replace("x", "*");
  expr.replace("X", "*");

  if (expr.length() == 0) {
    Serial.println(F("[ERROR]: Null query payload received."));
    return false;
  }

  // Validate strict bracket ordering [ { ( ) } ] via structural stack tracker
  char bracketStack[20];
  int stackTop = -1;

  for (int i = 0; i < expr.length(); i++) {
    char c = expr.charAt(i);

    // Validate illegal characters safely
    if (!isDigit(c) && c != '.' && c != '+' && c != '-' && c != '*' && c != '/' &&
        c != '(' && c != ')' && c != '{' && c != '}' && c != '[' && c != ']') {
      Serial.print(F("[SYNTAX ERROR]: Illegal character detected: '"));
      Serial.print(c);
      Serial.println(F("'"));
      return false;
    }

    // Process nested sequence bounds
    if (c == '[' || c == '{' || c == '(') {
      if (c == '{' && (stackTop == -1 || bracketStack[stackTop] != '[')) {
        Serial.println(F("[NESTING ERROR]: '{' used illegally. Must reside inside external '[' bounds."));
        return false;
      }
      if (c == '(' && (stackTop == -1 || bracketStack[stackTop] != '{')) {
        Serial.println(F("[NESTING ERROR]: '(' used illegally. Must reside inside external '{' bounds."));
        return false;
      }
      bracketStack[++stackTop] = c;
    } 
    else if (c == ']' || c == '}' || c == ')') {
      if (stackTop == -1) {
        Serial.println(F("[SYNTAX ERROR]: Closing delimiter has no opening counterpart."));
        return false;
      }
      char popped = bracketStack[stackTop--];
      if ((c == ']' && popped != '[') || (c == '}' && popped != '{') || (c == ')' && popped != '(')) {
        Serial.println(F("[SYNTAX ERROR]: Asymmetric boundary crossing. Mismatched bracket types."));
        return false;
      }
    }
  }

  if (stackTop != -1) {
    Serial.println(F("[SYNTAX ERROR]: Core arithmetic block left unclosed. Hanging bracket references."));
    return false;
  }
  return true;
}

// Converts characters into discrete memory structural tokens
bool tokenize(const String &expr, Token *tokens, int &count) {
  int i = 0;
  count = 0;

  while (i < expr.length()) {
    if (count >= MAX_TOKENS - 1) {
      Serial.println(F("[MEMORY OVERFLOW]: Equation is too long. Increase MAX_TOKENS buffer limit."));
      return false;
    }

    char c = expr.charAt(i);

    // Identify bracket identifiers
    if (c == '(' || c == '{' || c == '[' || c == ')' || c == '}' || c == ']') {
      tokens[count].type = (c == '(' || c == '{' || c == '[') ? TOKEN_OPEN_BRACKET : TOKEN_CLOSE_BRACKET;
      tokens[count].symbol = c;
      count++;
      i++;
    }
    // Identify operators
    else if (c == '+' || c == '-' || c == '*' || c == '/') {
      // Robust syntax protection: Prevent hanging operator patterns like "5 + * 3"
      if (count > 0 && tokens[count - 1].type == TOKEN_OP) {
        Serial.println(F("[SYNTAX ERROR]: Sequential operators found side-by-side."));
        return false;
      }
      tokens[count].type = TOKEN_OP;
      tokens[count].symbol = c;
      count++;
      i++;
    }
    // Parse floating-point numbers out of text blocks safely
    else if (isDigit(c) || c == '.') {
      String numBuffer = "";
      while (i < expr.length() && (isDigit(expr.charAt(i)) || expr.charAt(i) == '.')) {
        numBuffer += expr.charAt(i);
        i++;
      }
      tokens[count].type = TOKEN_NUMBER;
      tokens[count].value = numBuffer.toFloat();
      count++;
    }
  }
  
  // Tag end of stream boundary safely
  tokens[count].type = TOKEN_END;
  return true;
}

// DMAS Layer 3: Process Addition & Subtraction (Lowest Precedence)
float parseExpression(const Token *tokens, int &index, bool &success) {
  float result = parseTerm(tokens, index, success);
  if (!success) return 0;

  while (tokens[index].type == TOKEN_OP && (tokens[index].symbol == '+' || tokens[index].symbol == '-')) {
    char op = tokens[index].symbol;
    index++; // Step past operator token
    
    float nextTerm = parseTerm(tokens, index, success);
    if (!success) return 0;

    if (op == '+') result += nextTerm;
    if (op == '-') result -= nextTerm;
  }
  return result;
}

// DMAS Layer 2: Process Multiplication & Division (Higher Precedence)
float parseTerm(const Token *tokens, int &index, bool &success) {
  float result = parseFactor(tokens, index, success);
  if (!success) return 0;

  while (tokens[index].type == TOKEN_OP && (tokens[index].symbol == '*' || tokens[index].symbol == '/')) {
    char op = tokens[index].symbol;
    index++; // Step past operator token
    
    float nextFactor = parseFactor(tokens, index, success);
    if (!success) return 0;

    if (op == '*') result *= nextFactor;
    if (op == '/') {
      if (nextFactor == 0) {
        Serial.println(F("[MATH CRITICAL ERROR]: Hard division by absolute zero intercepted."));
        success = false;
        return 0;
      }
      result /= nextFactor;
    }
  }
  return result;
}

// DMAS Layer 1: Extract Base Quantities or Unpack Nested Sub-Expressions
float parseFactor(const Token *tokens, int &index, bool &success) {
  if (tokens[index].type == TOKEN_NUMBER) {
    float val = tokens[index].value;
    index++; // Advance token pointer
    return val;
  } 
  else if (tokens[index].type == TOKEN_OPEN_BRACKET) {
    index++; // Advance past opening boundary token
    float subResult = parseExpression(tokens, index, success);
    if (!success) return 0;

    if (tokens[index].type == TOKEN_CLOSE_BRACKET) {
      index++; // Advance past closing boundary token
      return subResult;
    } else {
      Serial.println(F("[FATAL ERROR]: Misaligned boundary tracking state."));
      success = false;
      return 0;
    }
  }

  Serial.println(F("[SYNTAX ERROR]: Expected numerical operand or bracket subset expression."));
  success = false;
  return 0;
}
