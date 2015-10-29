////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up Jason documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef JASON_DUMPER_H
#define JASON_DUMPER_H 1

#include <string>
#include <cmath>
#include <functional>

#include "Jason.h"
#include "JasonBuffer.h"
#include "JasonException.h"
#include "JasonSlice.h"
#include "JasonType.h"

namespace arangodb {
  namespace jason {

    // forward for fpconv function declared elsewhere
    int fpconv_dtoa (double fp, char dest[24]);

    // Dumps Jason into a JSON output string
    template<typename T, bool PrettyPrint = false>
    class JasonDumper {

      public:
    
        enum UnsupportedTypeStrategy {
          StrategyNullify,
          StrategyFail
        };

        JasonDumper (JasonDumper const&) = delete;
        JasonDumper& operator= (JasonDumper const&) = delete;

        JasonDumper (T& buffer, UnsupportedTypeStrategy strategy = StrategyFail) 
          : _buffer(&buffer), _strategy(strategy), _indentation(0) {
        }

        ~JasonDumper () {
        }

        friend std::ostream& operator<< (std::ostream& stream, JasonDumper const* dumper) {
          stream << *dumper->_buffer;
          return stream;
        }

        friend std::ostream& operator<< (std::ostream& stream, JasonDumper const& dumper) {
          stream << *dumper._buffer;
          return stream;
        }

        void setCallback (std::function<bool(T*, JasonSlice const*, JasonSlice const*)> const& callback) {
          _callback = callback;
        }

        void dump (JasonSlice const& slice) {
          _indentation = 0;
          internalDump(&slice, nullptr);
        }

        static void Dump (JasonSlice const& slice, T& buffer, UnsupportedTypeStrategy strategy = StrategyFail) {
          JasonDumper dumper(buffer, strategy);
          dumper.internalDump(&slice, nullptr);
        }

        static void Dump (JasonSlice const* slice, T& buffer, UnsupportedTypeStrategy strategy = StrategyFail) {
          JasonDumper dumper(buffer, strategy);
          dumper.internalDump(slice, nullptr);
        }
        
        static T Dump (JasonSlice const& slice, UnsupportedTypeStrategy strategy = StrategyFail) {
          T buffer;
          JasonDumper dumper(buffer, strategy);
          dumper.internalDump(&slice, nullptr);
          return buffer;
        }

        static T Dump (JasonSlice const* slice, UnsupportedTypeStrategy strategy = StrategyFail) {
          T buffer;
          JasonDumper dumper(buffer, strategy);
          dumper.internalDump(slice, nullptr);
          return buffer;
        }

        void reset () {
          _buffer->reset();
        }

        void append (JasonSlice const& slice) {
          internalDump(&slice, nullptr);
        }

        void append (JasonSlice const* slice) {
          internalDump(slice, nullptr);
        }

        void appendString (char const* src, JasonLength len) {
          _buffer->reserve(2 + len);
          _buffer->push_back('"');
          dumpString(src, len);
          _buffer->push_back('"');
        }

        void appendString (std::string const& str) {
          _buffer->reserve(2 + str.size());
          _buffer->push_back('"');
          dumpString(str.c_str(), str.size());
          _buffer->push_back('"');
        }

      private:

        void indent () {
          size_t n = _indentation;
          _buffer->reserve(n);
          for (size_t i = 0; i < n; ++i) {
            _buffer->append("  ", 2);
          }
        }

        void internalDump (JasonSlice const& slice, JasonSlice const* parent) {
          internalDump(&slice, parent);
        }

        void internalDump (JasonSlice const* slice, JasonSlice const* parent) {
          if (_callback && _callback(_buffer, slice, parent)) {
            return;
          }

          switch (slice->type()) {
            case JasonType::None: {
              handleUnsupportedType(slice);
              break; 
            }

            case JasonType::Null: {
              _buffer->append("null", 4);
              break;
            }

            case JasonType::Bool: {
              if (slice->getBool()) {
                _buffer->append("true", 4);
              } 
              else {
                _buffer->append("false", 5);
              }
              break;
            }

            case JasonType::Array: {
              JasonLength const n = slice->length();
              if (PrettyPrint) {
                _buffer->push_back('[');
                _buffer->push_back('\n');
                ++_indentation;
                for (JasonLength i = 0; i < n; ++i) {
                  indent();
                  internalDump(slice->at(i), slice);
                  if (i + 1 !=  n) {
                    _buffer->push_back(',');
                  }
                  _buffer->push_back('\n');
                }
                --_indentation;
                indent();
                _buffer->push_back(']');
              }
              else {
                _buffer->push_back('[');
                for (JasonLength i = 0; i < n; ++i) {
                  if (i > 0) {
                    _buffer->push_back(',');
                  }
                  internalDump(slice->at(i), slice);
                }
                _buffer->push_back(']');
              }
              break;
            }

            case JasonType::Object: {
              JasonLength const n = slice->length();
              if (PrettyPrint) {
                _buffer->push_back('{');
                _buffer->push_back('\n');
                ++_indentation;
                for (JasonLength i = 0; i < n; ++i) {
                  indent();
                  internalDump(slice->keyAt(i), slice);
                  _buffer->append(" : ", 3);
                  internalDump(slice->valueAt(i), slice);
                  if (i + 1 !=  n) {
                    _buffer->push_back(',');
                  }
                  _buffer->push_back('\n');
                }
                --_indentation;
                indent();
                _buffer->push_back('}');
              }
              else {
                _buffer->push_back('{');
                for (JasonLength i = 0; i < n; ++i) {
                  if (i > 0) {
                    _buffer->push_back(',');
                  }
                  internalDump(slice->keyAt(i), slice);
                  _buffer->push_back(':');
                  internalDump(slice->valueAt(i), slice);
                }
                _buffer->push_back('}');
              }
              break;
            }
            
            case JasonType::Double: {
              double const v = slice->getDouble();
              if (std::isnan(v) || ! std::isfinite(v)) {
                handleUnsupportedType(slice);
              }
              else {
                char temp[24];
                int len = fpconv_dtoa(v, &temp[0]);
                _buffer->append(&temp[0], static_cast<JasonLength>(len));
              }
              break; 
            }
            
            case JasonType::UTCDate: {
              handleUnsupportedType(slice);
              break;
            }

            case JasonType::External: {
              JasonSlice const external(slice->getExternal());
              internalDump(&external, nullptr);
              break;
            }
            
            case JasonType::MinKey:
            case JasonType::MaxKey: {
              handleUnsupportedType(slice);
              break;
            }
            
            case JasonType::Int:
            case JasonType::UInt:
            case JasonType::SmallInt: {
              dumpInteger(slice);
              break;
            }

            case JasonType::String: {
              JasonLength len;
              char const* p = slice->getString(len);
              _buffer->reserve(2 + len);
              _buffer->push_back('"');
              dumpString(p, len);
              _buffer->push_back('"');
              break;
            }

            case JasonType::Binary: {
              handleUnsupportedType(slice);
              break;
            }
            
            case JasonType::BCD: {
              // TODO
              handleUnsupportedType(slice);
              break;
            }
            
            case JasonType::Custom: {
              // TODO
              handleUnsupportedType(slice);
              break;
            }
          }
        }

        void dumpInteger (JasonSlice const* slice) {
          if (slice->isType(JasonType::UInt)) {
            uint64_t v = slice->getUInt();

            if (10000000000000000000ULL <= v) { _buffer->push_back('0' + (v / 10000000000000000000ULL) % 10); }
            if ( 1000000000000000000ULL <= v) { _buffer->push_back('0' + (v /  1000000000000000000ULL) % 10); }
            if (  100000000000000000ULL <= v) { _buffer->push_back('0' + (v /   100000000000000000ULL) % 10); }
            if (   10000000000000000ULL <= v) { _buffer->push_back('0' + (v /    10000000000000000ULL) % 10); }
            if (    1000000000000000ULL <= v) { _buffer->push_back('0' + (v /     1000000000000000ULL) % 10); }
            if (     100000000000000ULL <= v) { _buffer->push_back('0' + (v /      100000000000000ULL) % 10); }
            if (      10000000000000ULL <= v) { _buffer->push_back('0' + (v /       10000000000000ULL) % 10); }
            if (       1000000000000ULL <= v) { _buffer->push_back('0' + (v /        1000000000000ULL) % 10); }
            if (        100000000000ULL <= v) { _buffer->push_back('0' + (v /         100000000000ULL) % 10); }
            if (         10000000000ULL <= v) { _buffer->push_back('0' + (v /          10000000000ULL) % 10); }
            if (          1000000000ULL <= v) { _buffer->push_back('0' + (v /           1000000000ULL) % 10); }
            if (           100000000ULL <= v) { _buffer->push_back('0' + (v /            100000000ULL) % 10); }
            if (            10000000ULL <= v) { _buffer->push_back('0' + (v /             10000000ULL) % 10); }
            if (             1000000ULL <= v) { _buffer->push_back('0' + (v /              1000000ULL) % 10); }
            if (              100000ULL <= v) { _buffer->push_back('0' + (v /               100000ULL) % 10); }
            if (               10000ULL <= v) { _buffer->push_back('0' + (v /                10000ULL) % 10); }
            if (                1000ULL <= v) { _buffer->push_back('0' + (v /                 1000ULL) % 10); }
            if (                 100ULL <= v) { _buffer->push_back('0' + (v /                  100ULL) % 10); }
            if (                  10ULL <= v) { _buffer->push_back('0' + (v /                   10ULL) % 10); }

            _buffer->push_back('0' + (v % 10));
          } 
          else if (slice->isType(JasonType::Int)) {
            int64_t v = slice->getInt();
            if (v < 0) {
              _buffer->push_back('-');
            }
            if (v == INT64_MIN) {
              _buffer->append("9223372036854775808", 19);
              return;
            }
            v = -v;
          
            if (1000000000000000000LL <= v) { _buffer->push_back('0' + (v / 1000000000000000000LL) % 10); }
            if ( 100000000000000000LL <= v) { _buffer->push_back('0' + (v /  100000000000000000LL) % 10); }
            if (  10000000000000000LL <= v) { _buffer->push_back('0' + (v /   10000000000000000LL) % 10); }
            if (   1000000000000000LL <= v) { _buffer->push_back('0' + (v /    1000000000000000LL) % 10); }
            if (    100000000000000LL <= v) { _buffer->push_back('0' + (v /     100000000000000LL) % 10); }
            if (     10000000000000LL <= v) { _buffer->push_back('0' + (v /      10000000000000LL) % 10); }
            if (      1000000000000LL <= v) { _buffer->push_back('0' + (v /       1000000000000LL) % 10); }
            if (       100000000000LL <= v) { _buffer->push_back('0' + (v /        100000000000LL) % 10); }
            if (        10000000000LL <= v) { _buffer->push_back('0' + (v /         10000000000LL) % 10); }
            if (         1000000000LL <= v) { _buffer->push_back('0' + (v /          1000000000LL) % 10); }
            if (          100000000LL <= v) { _buffer->push_back('0' + (v /           100000000LL) % 10); }
            if (           10000000LL <= v) { _buffer->push_back('0' + (v /            10000000LL) % 10); }
            if (            1000000LL <= v) { _buffer->push_back('0' + (v /             1000000LL) % 10); }
            if (             100000LL <= v) { _buffer->push_back('0' + (v /              100000LL) % 10); }
            if (              10000LL <= v) { _buffer->push_back('0' + (v /               10000LL) % 10); }
            if (               1000LL <= v) { _buffer->push_back('0' + (v /                1000LL) % 10); }
            if (                100LL <= v) { _buffer->push_back('0' + (v /                 100LL) % 10); }
            if (                 10LL <= v) { _buffer->push_back('0' + (v /                  10LL) % 10); }

            _buffer->push_back('0' + (v % 10));
          }
          else if (slice->isType(JasonType::SmallInt)) {
            int64_t v = slice->getSmallInt();
            if (v < 0) {
              _buffer->push_back('-');
              v = -v;
            }
            _buffer->push_back('0' + v);
          }
          else {
            throw JasonException(JasonException::InternalError, "Unexpected number type");
          }
        }

        void dumpString (char const* src, JasonLength len) {
          static char const EscapeTable[256] = {
            //0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
            'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r', 'u', 'u', // 00
            'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', // 10
              0,   0, '"',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '/', // 20
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 30~4F
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0, // 50
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 60~FF
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
              0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
          };

          uint8_t const* p = reinterpret_cast<uint8_t const*>(src);
          uint8_t const* e = p + len;
          while (p < e) {
            uint8_t c = *p;

            if ((c & 0x80) == 0) {
              // check for control characters
              char esc = EscapeTable[c];

              if (esc) {
                _buffer->push_back('\\');
                _buffer->push_back(static_cast<char>(esc));

                if (esc == 'u') { 
                  uint16_t i1 = (((uint16_t) c) & 0xf0) >> 4;
                  uint16_t i2 = (((uint16_t) c) & 0x0f);

                  _buffer->append("00", 2);
                  _buffer->push_back(static_cast<char>((i1 < 10) ? ('0' + i1) : ('A' + i1 - 10)));
                  _buffer->push_back(static_cast<char>((i2 < 10) ? ('0' + i2) : ('A' + i2 - 10)));
                }
              }
              else {
                _buffer->push_back(static_cast<char>(c));
              }
            }
            else if ((c & 0xe0) == 0xc0) {
              // two-byte sequence
              if (p + 1 >= e) {
                throw JasonException(JasonException::InvalidUtf8Sequence);
              }

              _buffer->append(reinterpret_cast<char const*>(p), 2);
              ++p;
            }
            else if ((c & 0xf0) == 0xe0) {
              // three-byte sequence
              if (p + 2 >= e) {
                throw JasonException(JasonException::InvalidUtf8Sequence);
              }

              _buffer->append(reinterpret_cast<char const*>(p), 3);
              p += 2;
            }
            else if ((c & 0xf8) == 0xf0) {
              // four-byte sequence
              if (p + 3 >= e) {
                throw JasonException(JasonException::InvalidUtf8Sequence);
              }

              _buffer->append(reinterpret_cast<char const*>(p), 4);
              p += 3;
            }

            ++p;
          }
        }

        void handleUnsupportedType (JasonSlice const*) {
          if (_strategy == StrategyNullify) {
            _buffer->append("null", 4);
            return;
          }

          throw JasonException(JasonException::NoJsonEquivalent);
        }

      private:

        T* _buffer;
          
        std::function<bool(T*, JasonSlice const*, JasonSlice const*)> _callback;

        UnsupportedTypeStrategy _strategy;

        int _indentation;

    };

  }  // namespace arangodb::jason
}  // namespace arangodb

#endif