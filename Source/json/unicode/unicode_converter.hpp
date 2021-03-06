//
//  unicode_converter.hpp
//  
//
//  Created by Andreas Grosam on 1/27/12.
//  Copyright 2011 Andreas Grosam
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#ifndef JSON_UNICODE_UNICODE_CONVERTER_HPP
#define JSON_UNICODE_UNICODE_CONVERTER_HPP


#include "json/config.hpp"
#include "unicode_utilities.hpp"
#include "unicode_traits.hpp"
#include "unicode_errors.hpp"
#include "unicode_filter.hpp"

#include <boost/iterator/iterator_traits.hpp>
#include <boost/mpl/int.hpp>
#include <type_traits>
#include <algorithm>

#define JP_EXPECT(cond)  __builtin_expect(static_cast<long>(cond), 1)


// NO_STRICT_MAXIMAL_SUBPART
//
// If defined the conversion routines do not follow the "maximal 
// subpart of an ill-formed subsequence" rule. 
// D93b: Maximal subpart of an ill-formed subsequence: The longest code unit 
// subsequence starting at an unconvertible offset that is either:
//  a) the initial subsequence of a well-formed code unit sequence, or
//  b) a subsequence of length one.
// That means, if defined, the converting routine may iterate past the offset 
// which would have been a partial well-formed sequence.
//
// #define NO_STRICT_MAXIMAL_SUBPART


#if defined (NDEBUG)
//#define TEST_ACTION_FORCE_INLINE
//#define TEST_PARSE_FORCE_INLINE
//#define TEST_CONVERT_FORCE_INLINE
#endif



//  escaped_unicode_encoding_t
#pragma mark - escaped_unicode_encoding_t
namespace json { namespace unicode {
    
    
    // A special encodig form that results in escapded unicodes, effectively consisting of ASCII only characters.
    constexpr static struct escaped_unicode_encoding_t{}  escaped_unicode_encoding{};
    
}}



//  Base Template Class converter
#pragma mark - Base Template Class converter
    namespace json { namespace unicode {
    
    struct Validation {
        enum Flags {
            SAFE                    = 0,
            NO_CHECK_INPUT_RANGE    = 1 << 0,
            NO_VALIDATION           = 1 << 1, 
            UNSAFE                  = 0xFF
        };
    };
    
    struct Stateful {
        enum Option {
            No = false,
            Yes = true
        };
    };
    
    struct ParseOne {
        enum Option {
            No = false,
            Yes = true
        };
    };
    
    template <
        typename FromEncodingT, 
        typename ToEncodingT,
        Validation::Flags Validation = Validation::SAFE,
        Stateful::Option Stateful = static_cast<Stateful::Option>(std::conditional<
            encoding_traits<FromEncodingT>::is_multi_code_unit,
            boost::mpl::int_<Stateful::Yes>,
            boost::mpl::int_<Stateful::No>
        >::type::value),
        ParseOne::Option ParseOne = ParseOne::No,
        class Enable = void
    >
    struct converter;
//    {
//        static_assert(sizeof(Enable) == 0, "Base Template Shall Not Be Instantiated");
//
//        template <typename InIteratorT, typename OutIteratorT>
//        int 
//        convert(InIteratorT& first, InIteratorT last, OutIteratorT& des);
//    };
    
}}


// Base Template Class inernal::mb_state
namespace json { namespace unicode { namespace internal {
        
    
    template <typename Base, typename Derived>
    using IsBaseAndDerived = typename std::integral_constant<bool,
        std::is_base_of<Base, Derived>::value
        and !std::is_same<Base, Derived>::value
    >::type;
    
    
    
    template <bool Stateful>
    struct state {        
    };
    
    
    template <>
    struct state<false> {
        
        void set(int) { /*static_assert(0, "state is not mutable"); */}
        constexpr int get() const { return 0; }
    }; 
    
    
    
    template <>
    struct state<true> {
        
        typedef state<false> stateless;
        
        constexpr state() noexcept : state_(0) {}
        constexpr state(state const& other) noexcept : state_(other.state_) {}
        
        constexpr state(stateless const&) noexcept : state_(0) {}
        
        constexpr int get() const { return state_; }
        void    set(int state)  { state_ = state; }
    private:
        int state_;
    }; 
    
    
    
    // template class mb_state
    //
    // The mb_state is used to hold state information when parsing Unicode 
    // multibyte sequences (UTF-8, UTF-16). Namely it remembers the code units 
    // that constitute one Unicode character which have been read from the 
    // input iterator and which are then subsequently used in the parser's actions. 
    // Additionally, it (possibly) safes state information (an integer value) 
    // when parsing partially multibyte sequences. It is only required when the 
    // Unicode sequence is actually a multi byte sequence (UTF-8 and UTF-16) and 
    // when the parser shall be able to handle partial multi byte sequences in 
    // the input stream. Otherwise - in order to enable better optimization by 
    // the compiler - 'stateful' can be omitted.
    // 
    template <typename EncodingT, bool Stateful = true>
    class mb_state {
    public:
        typedef void buffer_type;
        
        mb_state() noexcept {}
        
        // Resets the state.
        // post condition: !(*this) == false
        void clear() {};        
        
        // Returns true if state is not equal start state, otherwise false.
        constexpr bool operator! () const { return false; }
        
        // Returns an integer representing the current state:
        constexpr int state() const { return 0; }
    };
    
    
}}}


//  Internal UTF-8 Parser
//  Validates an input sequence of UTF-8 and copies it or converts it to 
//  Unicode codepoints and copies it to output iterator.
#pragma mark - Internal - Basic UTF-8 Parser

//
//  UTF-8 Validation Traits
//
namespace json { namespace  unicode { namespace internal {
    
    using namespace json::unicode;  // unicode utilities
    using json::unicode::Validation;
    
    template <int L>
    struct utf8_validation_traits {
        static_assert(true, "INVALID_VALIDATION_FLAGS");
    };
    
    template <>
    struct utf8_validation_traits<static_cast<int>(Validation::SAFE)> {
        static constexpr bool no_check_input_range = false;
        static constexpr bool no_check_trails = false;
    };
    
    template <>
    struct utf8_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = false;
    };
    
    template <>
    struct utf8_validation_traits<static_cast<int>(Validation::NO_VALIDATION)> {
        static constexpr bool no_check_input_range = false;
        static constexpr bool no_check_trails = true;
    };
    
    template <>
    struct utf8_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE | Validation::NO_VALIDATION)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = true;
    };
    
    template <>
    struct utf8_validation_traits<static_cast<int>(Validation::UNSAFE)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = true;
    };
    
}}}
  
//
//  UTF-8 mb_state
//
namespace json { namespace unicode { namespace internal {
    
    
    
   
    template <bool Stateful>
    class mb_state<unicode::UTF_8_encoding_tag, Stateful> : private state<Stateful>
    {
        typedef state<Stateful> base;
        
    public:        
        typedef uint8_t buffer_type[4];
        
        mb_state() noexcept {}
        
        mb_state(const mb_state& other) noexcept : base(other) {
            buffer_[0] = other.buffer_[0];
            buffer_[1] = other.buffer_[1];
            buffer_[2] = other.buffer_[2];
            buffer_[3] = other.buffer_[3];
        }
        
        mb_state& operator=(const mb_state& other) {
            base::operator=(other);
            buffer_[0] = other.buffer_[0];
            buffer_[1] = other.buffer_[1];
            buffer_[2] = other.buffer_[2];
            buffer_[3] = other.buffer_[3];
            return *this;
        }
        
        void clear()                { base::set(0); }
        
        constexpr bool operator! () const { return base::get() != 0; }
        
        buffer_type& buffer()       { return buffer_; }
        buffer_type const& buffer() const { return buffer_; }
        
        void set(int state)         { base::set(state); }
        constexpr int get() const   { return base::get(); }
        
        
    private: 
        buffer_type buffer_;
    };
    
    
    
}}}

//
//  UTF-8 Parser Actions
//
namespace json { namespace unicode { namespace internal {
    
    // The boolean template parameter 'NoCheckLast' and 'NoCheckTrails'
    // determine the amount of checks performed in member function convert().
        
    
    //
    //  "actions" classes define the "semantic actions" upon successfully 
    //  parsing the UTF-8 unicode sequence.
    //
    struct utf8_noop_actions 
    {
        typedef bool result_type;
        typedef mb_state<UTF_8_encoding_tag>::buffer_type buffer_type;
        
        template <typename...Args>
        constexpr bool action_single_byte(const buffer_type&, Args&&...) const { return true; }
        
        template <typename...Args>
        constexpr bool action_double_byte(const buffer_type&, Args&&...) const { return true; }
        
        template <typename...Args>
        constexpr bool action_triple_byte(const buffer_type&, Args&&...) const { return true; }

        template <typename...Args>
        constexpr bool action_quad_byte(const buffer_type&, Args&&...) const { return true; }
    };
    
    
    template <typename OutIterator>
    struct utf8_to_codepoint_actions 
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_8_encoding_tag>::buffer_type buffer_type;

        constexpr utf8_to_codepoint_actions() noexcept = default;
        
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = static_cast<code_point_t>(buffer[0]);
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = ((static_cast<uint32_t>(buffer[0]) << 6) & 0x7FFu) + (buffer[1] & 0x3Fu);
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = ((static_cast<uint32_t>(buffer[0]) << 12) & 0xFFFFu)
            + ((static_cast<uint32_t>(buffer[1]) << 6) & 0xFFFu) 
            + (static_cast<uint32_t>(buffer[2]) & 0x3Fu);
            return true;
        }
        
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_quad_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ =  ((static_cast<uint32_t>(buffer[0]) << 18) & 0x1FFFFFu)
            + ((static_cast<uint32_t>(buffer[1]) << 12) & 0x3FFFFu)
            + ((static_cast<uint32_t>(buffer[2]) << 6) & 0xFFFu)
            + (buffer[3] & 0x3Fu);
            return true;
        }
        
    };
    
    
    template <typename OutIterator>
    struct utf8_copy_actions 
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_8_encoding_tag>::buffer_type buffer_type;
                
        constexpr utf8_copy_actions() noexcept = default;
                
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 2, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 3, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
//            *dest++ = buffer[2];
            return true;
        }
        
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_quad_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 4, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
//            *dest++ = buffer[2];
//            *dest++ = buffer[3];
            return true;
        }
    };
    
    template <typename OutIterator, typename OutEncodingT, class Enable = void>
    struct utf8_to_utf_actions;
//    {
//        static_assert( (boost::is_same<void,Enable>::value), "base template not instantiable");
//    };

    
    template <typename OutIterator, typename OutEncodingT>
    struct utf8_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, OutEncodingT>::value
        >::type
    >    
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_8_encoding_tag>::buffer_type buffer_type;
        
        utf8_to_utf_actions() noexcept {}
                
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 2, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 3, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
//            *dest++ = buffer[2];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_quad_byte(const buffer_type& buffer, OutIterator& dest) const {
            std::copy_n(&buffer[0], 4, dest);
//            *dest++ = buffer[0];
//            *dest++ = buffer[1];
//            *dest++ = buffer[2];
//            *dest++ = buffer[3];
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf8_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
    std::is_base_of<UTF_16_encoding_tag, OutEncodingT>::value
        >::type
    >    
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_8_encoding_tag>::buffer_type  buffer_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf8_to_utf_actions() noexcept  {}
                
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[0]));
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(
                    (((static_cast<uint16_t>(buffer[0]) << 6) & 0x7FFu) + (buffer[1] & 0x3Fu))));
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(
                  ((static_cast<uint32_t>(buffer[0]) << 12) & 0xFFFFu)
                  + ((static_cast<uint32_t>(buffer[1]) << 6) & 0xFFFu)
                  + (static_cast<uint32_t>(buffer[2]) & 0x3Fu)));
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_quad_byte(const buffer_type& buffer, OutIterator& dest) const {
            code_point_t cp = ((static_cast<uint32_t>(buffer[0]) << 18) & 0x1FFFFFu)
                            + ((static_cast<uint32_t>(buffer[1]) << 12) & 0x3FFFFu)
                            + ((static_cast<uint32_t>(buffer[2]) << 6) & 0xFFFu)
                            + (buffer[3] & 0x3Fu);
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_lead(cp)));
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_trail(cp)));
            return true;
        }
    };
    

    template <typename OutIterator, typename OutEncodingT>
    struct utf8_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_base_of<UTF_32_encoding_tag, OutEncodingT>::value
        >::type
    >    
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_8_encoding_tag>::buffer_type  buffer_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf8_to_utf_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint32_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(
                ((static_cast<uint32_t>(buffer[0]) << 6) & 0x7FFu) + (buffer[1] & 0x3Fu));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(
                     ((static_cast<uint32_t>(buffer[0]) << 12) & 0xFFFFu)
                     + ((static_cast<uint32_t>(buffer[1]) << 6) & 0xFFFu)
                     + (static_cast<uint32_t>(buffer[2]) & 0x3Fu));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_quad_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(
                     ((static_cast<uint32_t>(buffer[0]) << 18) & 0x1FFFFFu)
                     + ((static_cast<uint32_t>(buffer[1]) << 12) & 0x3FFFFu)
                     + ((static_cast<uint32_t>(buffer[2]) << 6) & 0xFFFu)
                     + (buffer[3] & 0x3Fu));
            return true;
        }
    };
}}}

//
//  UTF-8 Parser
//
namespace json {  namespace unicode { namespace internal {
    
    
    /**
     Implements static member function parse(). Class template parameters 
     specify the concrete validation and member function template parameters 
     determine the semantic actions (that is, copying or conversion to Unicode 
     code points or to other Unicode encoding forms). The state and buffer must 
     be provided by the caller.
    */    
    template <bool NoCheckLast, bool NoCheckTrails, bool Stateful, bool ParseOne = false>
    class utf8_parser
    {
    public:
        typedef mb_state<UTF_8_encoding_tag, Stateful>   mb_state_type;
        
    private:
        enum State {
            State_0 = 0,
            State_10, 
            State_20,
            State_21,
            State_30,
            State_31,
            State_32
        };
        
        mb_state_type mb_state_;
        
    public:
        typedef typename mb_state_type::buffer_type     buffer_type;
        
        utf8_parser() noexcept {};
        utf8_parser(const mb_state_type& state) noexcept : mb_state_(state) {}
                
        
        mb_state_type state() const { return mb_state_; }
        
        void clear() { mb_state_.clear(); }
                
        template <typename InIteratorT, class Actions, typename... Args>
        int
#if defined (TEST_PARSE_FORCE_INLINE)
        __attribute__((always_inline))
#endif                    
        parse(InIteratorT& first, InIteratorT last, Actions actions, Args&&... args)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename UTF_8_encoding_traits::code_unit_type), "");
            
            assert(mb_state_.get() >= 0);
            assert(!ParseOne or (first != last));
            
            buffer_type& b = mb_state_.buffer();
            
            int result;
            switch (mb_state_.get()) { 
                case State_0: goto state_0;
                case State_10: goto state_10;
                case State_20: goto state_20;
                case State_21: goto state_21;
                case State_30: goto state_30;
                case State_31: goto state_31;
                case State_32: goto state_32;
            }
        state_0:
            while (ParseOne or JP_EXPECT(first != last)) {
                b[0]= static_cast<uint8_t>(*first);
                if (utf8_is_single(b[0])) {
                    assert(mb_state_.get() == State_0);
                    // single byte
                    ++first;
                    if (!actions.action_single_byte(b, std::forward<Args>(args)...)) {
                        return PARSE_ACTION_BREAK;
                    }
                    if (ParseOne)
                        return 0;
                    else
                        continue;
                }
                if (b[0] < 0xE0u) {
                    if (b[0] >= 0xC2u) {
                        // two bytes
                        ++first;
                        if (JP_EXPECT(NoCheckLast or first != last)) {
                        state_10:
                            b[1] = static_cast<uint8_t>(*first);            
                            if (JP_EXPECT(NoCheckTrails or utf8_is_trail(b[1]))) {
                                ++first;
                                if (!actions.action_double_byte(b, std::forward<Args>(args)...)) {
                                    return PARSE_ACTION_BREAK;
                                }
                                mb_state_.set(State_0);
                                if (ParseOne)
                                    return 0;
                                else
                                    continue;
                            }  else { mb_state_.set(result = E_TRAIL_EXPECTED); } 
                        } else { mb_state_.set(State_10); result = E_UNEXPECTED_ENDOFINPUT; }
                    } else { mb_state_.set(result = E_INVALID_START_BYTE); }
                } 
                else if (b[0] < 0xF0u) { // three bytes
                    ++first;
                    if (JP_EXPECT(NoCheckLast or first != last)) {
                    state_20:
                        b[1] = static_cast<uint8_t>(*first);
                        if (JP_EXPECT(NoCheckTrails
                                      or ((b[0] == 0xE0u) and ((b[1] - 0xA0u) <= (0xBFu-0xA0u)))
                                      or ((b[0] == 0xEDu) and ((b[1] - 0x80u) <= (0x9Fu-0x80u)))
                                      or ( b[0] != 0xE0u and b[0] != 0xEDu and utf8_is_trail(b[1])) ))  
                        {
                            ++first;
                            if (JP_EXPECT(NoCheckLast or first != last)) {
                            state_21:
                                b[2] = static_cast<uint8_t>(*first);
                                if (JP_EXPECT(NoCheckTrails or utf8_is_trail(b[2]))) {
                                    ++first;
                                    if (!actions.action_triple_byte(b, std::forward<Args>(args)...)) {
                                        return PARSE_ACTION_BREAK;
                                    }
                                    mb_state_.set(State_0);
                                    if (ParseOne)
                                        return 0;
                                    else
                                        continue;
                                } else { mb_state_.set(result = E_TRAIL_EXPECTED);}
                            } else { mb_state_.set(State_21); result = E_UNEXPECTED_ENDOFINPUT;} 
                        } else {mb_state_.set(result = E_TRAIL_EXPECTED); /* or E_UNCONVERTABLE_OFFSET */ }
                    } else { mb_state_.set(State_20); result = E_UNEXPECTED_ENDOFINPUT; }
                } else  if (b[0] < 0xF5u) { // four bytes
                    ++first;
                    if (JP_EXPECT(NoCheckLast or first != last)) {
                    state_30:
                        b[1] = static_cast<uint8_t>(*first);
                        if (JP_EXPECT(NoCheckTrails
                                  or ((b[0] == 0xF0u) and ((b[1] - 0x90u) <= (0xBFu-0x90u)))
                                  or ((b[0] == 0xF4u) and ((b[1] - 0x80u) <= (0x8Fu-0x80u)))
                                  or ( b[0] != 0xF0u and b[0] != 0xF4u and utf8_is_trail(b[1])))) 
                        {
                            ++first;
                            if (JP_EXPECT(NoCheckLast or first != last)) {
                            state_31:
                                b[2] = static_cast<uint8_t>(*first);
                                if (JP_EXPECT(NoCheckTrails or utf8_is_trail(b[2]))) {
                                    ++first;
                                    if (JP_EXPECT(NoCheckLast or first != last)) {
                                    state_32:
                                        b[3] = static_cast<uint8_t>(*first);
                                        if (JP_EXPECT(NoCheckTrails or utf8_is_trail(b[3]))) {
                                            ++first;
                                            if (!actions.action_quad_byte(b, std::forward<Args>(args)...)) {
                                                return PARSE_ACTION_BREAK;
                                            }
                                            mb_state_.set(State_0);
                                            if (ParseOne)
                                                return 0;
                                            else
                                                continue;
                                        } else { mb_state_.set(result = E_TRAIL_EXPECTED); }
                                    } else { mb_state_.set(State_32); result = E_UNEXPECTED_ENDOFINPUT;} 
                                } else { mb_state_.set(result = E_TRAIL_EXPECTED); }
                            } else { mb_state_.set(State_31); result = E_UNEXPECTED_ENDOFINPUT; } 
                        } else { mb_state_.set(result = E_TRAIL_EXPECTED); /* E_UNCONVERTABLE_OFFSET */ }
                    } else { mb_state_.set(State_30); result = E_UNEXPECTED_ENDOFINPUT;} 
                } 
                else { mb_state_.set(result = E_INVALID_START_BYTE); }
                
                return result; // if we reach here it indicates and error during the conversion.
                
            } // while
            
            assert(mb_state_.get() == State_0);
            return 0;  // if we reach here, we had success or the sequence was empty
        }
        
        
    };  // class utf8_parser
    
    
}}} // namespace json::unicode::internal




//  Internal UTF-16 Parser
//  Validates an input sequence of UTF-16 and copies it or converts it to 
//  Unicode codepoints and copies it to output iterator.
#pragma mark - Internal - Basic UTF-16 Parser

//
//  Internal UTF-16 Validation Traits
//
namespace json { namespace unicode { namespace internal {

    template <int L>
    struct utf16_validation_traits {
        static_assert(true, "INVALID_VALIDATION_FLAGS");
    };
    
    template <>
    struct utf16_validation_traits<static_cast<int>(Validation::SAFE)> {
        static constexpr bool no_check_input_range = false;
        static constexpr bool no_check_trails = false;
    };
    
    template <>
    struct utf16_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = false;
    };
    
    template <>
    struct utf16_validation_traits<static_cast<int>(Validation::NO_VALIDATION)> {
        static constexpr bool no_check_input_range = false;
        static constexpr bool no_check_trails = true;
    };
    
    template <>
    struct utf16_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE | Validation::NO_VALIDATION)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = true;
    };
    
    template <>
    struct utf16_validation_traits<static_cast<int>(Validation::UNSAFE)> {
        static constexpr bool no_check_input_range = true;
        static constexpr bool no_check_trails = true;
    };

}}}

//
//  Internal UTF-16 mb_state
//
namespace json { namespace unicode { namespace internal {

    template <bool Stateful>
    class mb_state<UTF_16_encoding_tag, Stateful> : private state<Stateful>
    {   
        typedef state<Stateful> base;
    public:
        typedef uint16_t buffer_type[2];
        
        mb_state() noexcept {}
        
        mb_state(const mb_state& other) noexcept : base(other) {
            buffer_[0] = other.buffer_[0];
            buffer_[1] = other.buffer_[1];
        }
        
        mb_state& operator=(const mb_state& other) {
            base::operator=(other);
            buffer_[0] = other.buffer_[0];
            buffer_[1] = other.buffer_[1];
            return *this;
        }
        
        void clear()                { base::set(0); }
        
        bool operator! () const     { return base::get() != 0; }   
        
        buffer_type& buffer()       { return buffer_; }
        buffer_type const& buffer() const { return buffer_; }
        
        void set(int state)         { base::set(state); }
        int get() const             { return base::get(); }
                
    private:
        buffer_type buffer_;
    };
    
    
        
}}}

//
//  Internal UTF-16 Parser Actions
//
namespace json { namespace unicode { namespace internal {

    
    //
    //  "actions" classes define the "semantic actions" upon successfully 
    //  parsing the UTF-16 unicode sequence.
    //
    struct utf16_noop_actions 
    {
        typedef bool result_type;
        typedef mb_state<UTF_16_encoding_tag>::buffer_type buffer_type;
        
        template <typename... Args>
        constexpr result_type action_single_byte(const buffer_type&, Args&&...) const { return true; }
        
        template <typename... Args>
        constexpr result_type action_double_byte(const buffer_type&, Args&&...) const { return true; }
        
        template <typename... Args>
        constexpr result_type action_triple_byte(const buffer_type&, Args&&...) const { return true; }
        
        template <typename... Args>
        constexpr result_type action_surrogate_pair(const buffer_type&, Args&&...) const { return true; }
    };
    
    
    template <typename OutIterator>
    struct utf16_to_codepoint_actions 
    {
        typedef bool result_type;
        typedef mb_state<UTF_16_encoding_tag>::buffer_type  buffer_type;
        typedef typename host_endianness::type              host_endian_t;
        
        
        constexpr utf16_to_codepoint_actions() noexcept = default;
                
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = static_cast<code_point_t>(buffer[0]);
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = static_cast<code_point_t>(buffer[0]);
            return true;
        }
        
        result_type 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = static_cast<code_point_t>(buffer[0]);
            return true;
        }
        
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_surrogate_pair(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = utf16_surrogate_pair_to_code_point(buffer[0], buffer[1]);
            return true;
        }
        
    };
    
    
    template <typename OutIterator>
    struct utf16_copy_actions 
    {
        typedef bool result_type;
        typedef mb_state<UTF_16_encoding_tag>::buffer_type buffer_type;
        
        constexpr utf16_copy_actions() noexcept = default;

        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_surrogate_pair(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = buffer[0];
            *dest++ = buffer[1];
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT, class Enable = void>
    struct utf16_to_utf_actions;
//    {
//        static_assert( (std::is_same<void,Enable>::value), "base template not instantiable");
//    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf16_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, OutEncodingT>::value
        >::type
    >
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_16_encoding_tag>::buffer_type     buffer_type;
        typedef typename host_endianness::type                          host_endian_t;
        typedef UTF_8_encoding_tag                                      to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::code_unit_type code_unit_t;
        
        
        constexpr utf16_to_utf_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            assert( buffer[0] <= 0x7Fu );
            *dest++ = static_cast<uint8_t>(buffer[0]);
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            // UTF-16 to code point:
            code_point_t cp = buffer[0];
            assert(cp <= 0x7FFu and cp > 0x7Fu);
            // code point to UTF-8:
            *dest++ = static_cast<code_unit_t>(((cp >> 6) | 0xC0u));
            *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            code_point_t cp = buffer[0];
            assert(cp <= 0xFFFFu and cp > 0x7FFu);
            *dest++ = static_cast<code_unit_t>(((cp >> 12) | 0xE0u));
            *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
            *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
            return true;
        }
        
        bool 
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_surrogate_pair(const buffer_type& buffer, OutIterator& dest) const {
            code_point_t cp = utf16_surrogate_pair_to_code_point(buffer[0], buffer[1]);
            *dest++ = static_cast<code_unit_t>(((cp >> 18) | 0xF0u));
            *dest++ = static_cast<code_unit_t>((((cp >> 12) & 0x3Fu) | 0x80u));
            *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
            *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf16_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_base_of<UTF_16_encoding_tag, OutEncodingT>::value
        >::type
    > 
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_16_encoding_tag>::buffer_type buffer_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf16_to_utf_actions() noexcept = default;
        
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_surrogate_pair(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[0]));
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(buffer[1]));
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf16_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_base_of<UTF_32_encoding_tag, OutEncodingT>::value
        >::type
    > 
    {
        typedef bool result_type;
        typedef typename mb_state<UTF_16_encoding_tag>::buffer_type buffer_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf16_to_utf_actions() noexcept = default;
   
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_single_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint32_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_double_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint32_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif
        action_triple_byte(const buffer_type& buffer, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint32_t>(buffer[0]));
            return true;
        }
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_surrogate_pair(const buffer_type& buffer, OutIterator& dest) const {
            code_point_t cp = utf16_surrogate_pair_to_code_point(buffer[0], buffer[1]);            
            *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint32_t>(cp));
            return true;
        }
    };
    

}}}


//
//  Internal UTF-16 Parser 
//
namespace json { namespace unicode { namespace internal {
    
    using namespace json::unicode;  // unicode utilities
    
    
    // The boolean template parameter 'NoCheckLast' and 'NoCheckTrails'
    // determine the amount of checks performed in member function convert().
    template <typename EndianT, bool NoCheckLast, bool NoCheckTrails, bool Stateful, bool ParseOne = false>
    class utf16_parser
    {
    public:
        typedef mb_state<UTF_16_encoding_tag, Stateful>  mb_state_type;
        
    private:
        enum State {
            State_0 = 0,
            State_1 
        };
            
        mb_state_type mb_state_;
        
        
    public:
        
        utf16_parser() noexcept {}
        utf16_parser(const mb_state_type& state) noexcept : mb_state_(state) {}
        

        mb_state_type state() const { return mb_state_; }        
        
        void clear() { mb_state_.clear(); }
        
        template <typename InIteratorT, typename Actions, typename... Args>
        int
#if defined (TEST_PARSE_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        parse(InIteratorT& first, InIteratorT last, Actions actions, Args&&... args)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename UTF_16_encoding_traits::code_unit_type), "");
            
            typedef EndianT                             from_endian_t;
            typedef typename host_endianness::type      host_endian_t;
            typedef typename mb_state_type::buffer_type buffer_type;
            typedef typename UTF_16_encoding_traits::code_unit_type code_unit_type;
            
            assert(mb_state_.get() == State_0 or mb_state_.get() == State_1);
            assert(!ParseOne or (first != last));
            
            
            buffer_type& b = mb_state_.buffer();

            if (mb_state_.get() == State_1) 
                goto state_1;            
            
            while (ParseOne or first != last)
            {
                b[0] = byte_swap<from_endian_t, host_endian_t>(static_cast<code_unit_type>(*first));
                if (b[0] <= 0x7Fu) {
                    assert(mb_state_.get() == State_0);
                    ++first;
                    if (!actions.action_single_byte(b, std::forward<Args>(args)...)) {
                        return PARSE_ACTION_BREAK;                        
                    }
                    if (ParseOne) 
                        return 0;
                    else 
                        continue;
                }
                else if (b[0] <= 0x7FFu) {                    
                    assert(mb_state_.get() == State_0);
                    ++first;
                    if (!actions.action_double_byte(b, std::forward<Args>(args)...)) {
                        return PARSE_ACTION_BREAK;                        
                    }
                    if (ParseOne) 
                        return 0;
                    else 
                        continue;
                }
                else if (utf16_is_single(b[0])) {
                    assert(mb_state_.get() == State_0);
                    ++first;
                    if (!actions.action_triple_byte(b, std::forward<Args>(args)...)) {
                        return PARSE_ACTION_BREAK;                        
                    }
                    if (ParseOne) 
                        return 0;
                    else 
                        continue;
                }
                else if (NoCheckTrails or utf16_is_lead(b[0])) {
                    ++first;
                    if (NoCheckLast or first != last) {
                    state_1:
                        b[1] = byte_swap<from_endian_t, host_endian_t>(static_cast<code_unit_type>(*first));
                        if (NoCheckTrails or utf16_is_trail(b[1])) {
                            ++first;
                            if (!actions.action_surrogate_pair(b, std::forward<Args>(args)...)) {
                                return PARSE_ACTION_BREAK;                        
                            }
                            mb_state_.set(State_0);
                            if (ParseOne)
                                return 0;
                            else 
                                continue;
                        } else { return E_TRAIL_EXPECTED; }
                    }  else { mb_state_.set(State_1); return E_UNEXPECTED_ENDOFINPUT; }
                } else { return E_INVALID_START_BYTE; }
            }
            return 0;
        }
        
    }; // class utf16_parser
    
}}}




//
//  Internal UTF-32 Validation Traits
//
namespace json { namespace unicode { namespace internal {

    template <int L>
    struct utf32_validation_traits {
        static_assert(true, "INVALID_VALIDATION_FLAGS");
    };
    
    template <>
    struct utf32_validation_traits<static_cast<int>(Validation::SAFE)> {
        static constexpr bool no_check_valid_unicode = false;
    };
    
    template <>
    struct utf32_validation_traits<static_cast<int>(Validation::NO_VALIDATION)> {
        static constexpr bool no_check_valid_unicode = true;
    };
    
    template <>
    struct utf32_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE)> {
        static constexpr bool no_check_valid_unicode = false;
    };
    
    template <>
    struct utf32_validation_traits<static_cast<int>(Validation::NO_CHECK_INPUT_RANGE | Validation::NO_VALIDATION)> {
        static constexpr bool no_check_valid_unicode = true;
    };
    
    template <>
    struct utf32_validation_traits<static_cast<int>(Validation::UNSAFE)> {
        static constexpr bool no_check_valid_unicode = true;
    };
    

}}}

//
//  Internal UTF-32 Parser Actions
//
namespace json { namespace unicode { namespace internal {
   
    //
    //  "actions" classes define the "semantic actions" upon successfully 
    //  parsing the UTF-32 unicode sequence.
    //
    struct utf32_noop_actions 
    {
        typedef bool result_type;
        // Parameter value shall be in platform endianness
        template <typename... Args>
        constexpr bool action_utf32(const uint32_t& value, Args&&...) const { return true; }
    };
    
    
    template <typename OutIterator>
    struct utf32_to_codepoint_actions 
    {
        typedef bool result_type;
        typedef typename host_endianness::type  host_endian_t;
        
        constexpr utf32_to_codepoint_actions() noexcept = default;
                
        result_type
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_utf32(const uint32_t& value, OutIterator& dest) const {
            *dest++ = static_cast<code_point_t>(value);
            return true;
        }
        
    };
    
    
    template <typename OutIterator>
    struct utf32_copy_actions 
    {
        typedef bool result_type;
        
        constexpr utf32_copy_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_utf32(const uint32_t& value, OutIterator& dest) const {
            *dest++ = value;
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT, class Enable = void>
    struct utf32_to_utf_actions;
//    {
//        static_assert( (std::is_same<void,Enable>::value), "base template not instantiable");
//    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf32_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, OutEncodingT>::value
        >::type
    > 
    {
        typedef bool result_type;
        typedef typename host_endianness::type                          host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type             to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag     to_endian_t;
        typedef typename encoding_traits<to_encoding_t>::code_unit_type code_unit_t;
        
        
        constexpr utf32_to_utf_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_utf32(const uint32_t& value, OutIterator& dest) const {
            // UTF-16 to code point:
            code_point_t cp = static_cast<code_point_t>(value);
            // code point to UTF-8:
            int len = utf8_encoded_length_unsafe(cp);
            switch (len) {
                case 1:
                    *dest++ = cp;
                    break;
                case 2:
                    *dest++ = static_cast<code_unit_t>(((cp >> 6) | 0xC0u));
                    *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                    break;
                case 3: 
                    *dest++ = static_cast<code_unit_t>(((cp >> 12) | 0xE0u));
                    *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
                    *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                    break;
                case 4:
                    *dest++ = static_cast<code_unit_t>(((cp >> 18) | 0xF0u));
                    *dest++ = static_cast<code_unit_t>((((cp >> 12) & 0x3Fu) | 0x80u));
                    *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
                    *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                    break;                    
            }
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf32_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_base_of<UTF_16_encoding_tag, OutEncodingT>::value
        >::type
    >
    {
        typedef bool result_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf32_to_utf_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_utf32(const uint32_t& value, OutIterator& dest) const {
            code_point_t cp = static_cast<code_point_t>(value);
            int len = utf16_encoded_length_unsafe(cp);
            switch (len) {
                case 1:
                    *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(cp));
                    break;
                case 2:
                    *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_lead(cp)));
                    *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_trail(cp)));
                    break;
            }
            return true;
        }
    };
    
    
    template <typename OutIterator, typename OutEncodingT>
    struct utf32_to_utf_actions<OutIterator, OutEncodingT,
        typename std::enable_if<
            std::is_base_of<UTF_32_encoding_tag, OutEncodingT>::value
        >::type
    > 
    {
        typedef bool result_type;
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<OutEncodingT>::type         to_encoding_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        constexpr utf32_to_utf_actions() noexcept = default;
        
        bool
#if defined (TEST_ACTION_FORCE_INLINE)
        __attribute__((always_inline))
#endif            
        action_utf32(const uint32_t& value, OutIterator& dest) const {
            *dest++ = byte_swap<host_endian_t, to_endian_t>(value);
            return true;
        }
    };
    
    
}}}


//  Internal UTF-32 Parser
//  Validates an input sequence of UTF-32 and copies it or converts it to 
//  Unicode codepoints and copies it to output iterator.
#pragma mark - Internal - Basic UTF-32 Parser
namespace json { namespace unicode { namespace internal {
    
    using namespace json::unicode;  // unicode utilities
    
    // The boolean template parameter 'NoCheckLast' and 'NoCheckTrails'
    // determine the amount of checks performed in member function convert().
        
    template <typename EndianT, bool NoCheckValidUnicode, bool ParseOne>
    class utf32_parser 
    {
        typedef typename host_endianness::type      host_endian_t;
        typedef EndianT                             from_endian_t;
        
    public:
        template <typename InIteratorT, typename Actions, typename... Args>
        int
#if defined (TEST_PARSE_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        parse(InIteratorT& first, InIteratorT last, Actions actions, Args&&... args)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename UTF_32_encoding_traits::code_unit_type), "");
            
            while (ParseOne or first != last)
            {
                uint32_t cu = byte_swap<from_endian_t, host_endian_t>(static_cast<uint32_t>(*first));
                if (NoCheckValidUnicode or isUnicodeScalarValue(cu)) {
                    if (!actions.action_utf32(cu, std::forward<Args>(args)...)) {
                        return PARSE_ACTION_BREAK;
                    }
                    if (ParseOne) {
                        return 0;
                    }                    
                } else {
                    return E_INVALID_UNICODE;
                }
                ++first;
            }
            return 0;
        }
        
    }; // class utf32_parser
    
}}}









//
//  Class converter  specializations
//
#pragma mark - Class Template Specializations for UTF-8
namespace json {  namespace unicode {
    
        
#pragma mark - converter <UTF-8 to Unicode code point>
    //
    // struct converter
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful, ParseOne,
        typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, FromEncodingT>::value
            and std::is_same<code_point_t, ToEncodingT>::value
        >::type
    > 
    {        
        
        typedef internal::utf8_parser<
            internal::utf8_validation_traits<Validation>::no_check_input_range, 
            internal::utf8_validation_traits<Validation>::no_check_trails,
            Stateful, ParseOne
        >                                                           parser_type;        
        
    public:
        typedef typename parser_type::mb_state_type                 mb_state_type; 
        
    private:
        parser_type parser_;
        
    public:
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        
        mb_state_type state() const { return parser_.mb_state(); }

        void clear() { parser_.clear(); }
        
 
        /**        
         Convert a sequence of Unicode characters in encoding form UTF-8
         to a corresponding sequence of Unicode code points writing it into
         output iterator 'dest'.
         The converter keeps state information required to parse partial 
         multibyte sequences.

         If the converter starts with an initial input sequence, the state of
         the converter shall be cleared. The state is cleared after construction
         or after clear_state() has been called.

         If the sequence stops at a partial multibyte sequence - which was 
         valid up to that point, the function returns E_UNEXPECTED_ENDOFINPUT 
         and the conversion state is set such that a subsequent call of 
         convert() can be proceed with the continuing sequence starting at 
         the offset immediately following the last byte. The sequences can be
         divided at any code unit boundary.

         Returns zero if conversion was successful and complete. Otherwise
         returns E_UNEXPECTED_ENDOFINPUT indicating a partial multi-byte 
         sequecne at the end, otherwise a negative number indicating any 
         other error.
         
         Errors:
              E_TRAIL_EXPECTED:           trail byte expected or trail byte yields an unconvertable offset.
              E_UNEXPECTED_ENDOFINPUT:    unexpected and of input
              E_INVALID_START_BYTE:       invalid start byte
              E_INVALID_CODE_POINT:       detected surrogate or code point is invalid Unicode 
             

         According the "Constraints on Conversion process" in the Unicode
         Standard v6, this algorithm conforms to the concept of "maximal sub-
         part".

         Safe version:

         - checks for EOF
         - checks for valid UTF input


         Unsafe version:

         The Unicode sequence shall be well-formed, otherwise the result is
         undefined.
         
          - does not check for valid start byte
          - does not check for valid trail byte
          - does not check for valid number of trail bytes.
          - does not ensure that first equals last on exit
          - may crash if input is not well-formed.
        */

        template <typename InIteratorT, typename OutIteratorT>
        int convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            typedef typename internal::utf8_to_codepoint_actions<OutIteratorT> actions_t;
            
            int result = parser_.parse(first, last, actions_t(), dest);
            return result;
        }
        
    };  // class converter UTF-8 to codepoints
    
    
    
        
#pragma mark converter <Unicode code point to UTF-8>
    //
    // struct converter  
    //    
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful::No, ParseOne,
        typename std::enable_if<
            std::is_same<code_point_t, FromEncodingT>::value
            and std::is_same<UTF_8_encoding_tag, ToEncodingT>::value
        >::type
    > 
    {   
        void clear() {}
        
        /**        
        Convert a sequence of Unicode code points using UTF-8 encoding and 
        copy the result into the sequence starting at dest.    
        The output sequence must be capable to hold the number of generated
        code units.

        Returns zero on success, or a negative number indicating the kind of
        error.

        Invalid Unicode code points (>0+10FFFF) and surrogates will not be 
        converted, in which case the result equals E_INVALID_CODE_POINT.

        All other code points (noncharacters, etc.) will be converted.        

        Errors:
        E_INVALID_CODE_POINT:   The code point is not in the range of valid
                                Unicode.
        */ 
        template <typename InIteratorT, typename OutIteratorT>
        static int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)        
        {            
            typedef FromEncodingT  code_unit_t;
            
            assert(!ParseOne or (first != last));
            
            while (ParseOne or first != last) 
            {            
                // encode the Unicode code point to a UTF-8 byte sequence
                code_unit_t cp = static_cast<code_unit_t>(*first);
                int len = utf8_encoded_length(cp); // returns 0 if code_point is a surrogate or not a valid Unicode code point
                switch (len) {
                    case 1:
                        *dest++ = cp; ++first;
                        if (ParseOne) 
                            return 0;
                        else
                            continue;
                    case 2:
                        *dest++ = static_cast<code_unit_t>(((cp >> 6) | 0xC0u));
                        *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                        ++first;
                        if (ParseOne) 
                            return 0;
                        else
                            continue;
                    case 3: 
                        *dest++ = static_cast<code_unit_t>(((cp >> 12) | 0xE0u));
                        *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
                        *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                        ++first;
                        if (ParseOne) 
                            return 0;
                        else
                            continue;
                    case 4:
                        *dest++ = static_cast<code_unit_t>(((cp >> 18) | 0xF0u));
                        *dest++ = static_cast<code_unit_t>((((cp >> 12) & 0x3Fu) | 0x80u));
                        *dest++ = static_cast<code_unit_t>((((cp >> 6) & 0x3Fu) | 0x80u));
                        *dest++ = static_cast<code_unit_t>(((cp & 0x3Fu) | 0x80u));
                        ++first;
                        if (ParseOne) 
                            return 0;
                        else
                            continue;
                    default: 
                        return E_INVALID_CODE_POINT;                    
                }
            }
            return 0;
        }
        
        
    };
    
   
#pragma mark Struct converter <UTF-8 to any UTF>
    //
    // struct converter
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful, ParseOne, 
        typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, FromEncodingT>::value
            and std::is_base_of<utf_encoding_tag, ToEncodingT>::value
        >::type
    >
    {                
        typedef internal::utf8_parser<
            internal::utf8_validation_traits<Validation>::no_check_input_range, 
            internal::utf8_validation_traits<Validation>::no_check_trails,
            Stateful, ParseOne
        >                                                           parser_type;
        
        // Upgrade to_encoding:
        typedef typename add_endianness<ToEncodingT>::type          to_encoding;

    public:
        typedef typename parser_type::mb_state_type                 mb_state_type;
                
    private:   
        parser_type parser_;
        
    public:
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        

        mb_state_type state() const { return parser_.state(); }

        void clear() { parser_.clear(); }
        
#pragma mark UTF-8 sequence to UTF-xx        
        /**
        Convert a sequence of Unicode characters in encoding form UTF-8
        to a corresponding sequence of Unicode code units encoded in 
        'to_encoding' and write it into output iterator 'dest'.
        The converter keeps state information required to parse partial 
        multibyte sequences.

        If the converter starts with an initial input sequence, the state of
        the converter shall be cleared. The state is cleared after construction
        or after clear_state() has been called.

        If the sequence stops at a partial multibyte sequence - which was 
        valid up to that point, the function returns E_UNEXPECTED_ENDOFINPUT 
        and the conversion state is set such that a subsequent call of 
        convert() can be proceed with the continuing sequence starting at 
        the offset immediately following the last byte. The sequences can be
        divided at any code unit boundary.

        Returns zero if conversion was successful and complete. Otherwise
        returns E_UNEXPECTED_ENDOFINPUT indicating a partial multi-byte 
        sequecne at the end, otherwise a negative number indicating any 
        other error.

        Errors:
          E_TRAIL_EXPECTED:           trail byte expected or trail byte yields an unconvertable offset.
          E_UNEXPECTED_ENDOFINPUT:    unexpected and of input
          E_INVALID_START_BYTE:       invalid start byte
          E_INVALID_CODE_POINT:       detected surrogate or code point is invalid Unicode 
         

        According the "Constraints on Conversion process" in the Unicode
        Standard v6, this algorithm conforms to the concept of "maximal sub-
        part".

        Safe version:

        - checks for EOF
        - checks for valid UTF input


        Unsafe version:

        The Unicode sequence shall be well-formed, otherwise the result is
        undefined.

        - does not check for valid start byte
        - does not check for valid trail byte
        - does not check for valid number of trail bytes.
        - does not ensure that first equals last on exit
        - may crash if input is not well-formed.
        */
        template <typename InIteratorT, typename OutIteratorT>
        int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            typedef typename internal::utf8_to_utf_actions<OutIteratorT, to_encoding> actions_t;
            
            int result = parser_.parse(first, last, actions_t(), dest);
            return result;
        }
        
    };  // class converter UTF-8 to UTF
    

    
    static constexpr char __hex_table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    
    
    
#pragma mark Struct converter <UTF-8 to Escaped Unicode Sequence>
    //
    // struct converter
    //
    template <typename FromEncodingT, Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne>
    struct converter<FromEncodingT, escaped_unicode_encoding_t, Validation, Stateful, ParseOne,
    typename std::enable_if<
            std::is_same<UTF_8_encoding_tag, FromEncodingT>::value
        >::type
    >
    {
        typedef internal::utf8_parser<
            internal::utf8_validation_traits<Validation>::no_check_input_range,
            internal::utf8_validation_traits<Validation>::no_check_trails,
            Stateful, true/*parse one*/
        >                                                           parser_type;

        typedef UTF_8_encoding_tag                                  to_encoding;
        
    public:
        typedef typename parser_type::mb_state_type                 mb_state_type;
        
    private:
        parser_type parser_;
        
    public:
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        
        
        mb_state_type state() const { return parser_.state(); }
        
        void clear() { parser_.clear(); }
        
#pragma mark UTF-8 sequence to Escapde Unicode sequence
        /**
         Convert a sequence of Unicode characters in encoding form UTF-8
         to a corresponding sequence of Escapded Unicode sequences in
         UTF-8 and write it into output iterator 'dest'.
         The converter keeps state information required to parse partial
         multibyte sequences.
         
         If the converter starts with an initial input sequence, the state of
         the converter shall be cleared. The state is cleared after construction
         or after clear_state() has been called.
         
         If the sequence stops at a partial multibyte sequence - which was
         valid up to that point, the function returns E_UNEXPECTED_ENDOFINPUT
         and the conversion state is set such that a subsequent call of
         convert() can be proceed with the continuing sequence starting at
         the offset immediately following the last byte. The sequences can be
         divided at any code unit boundary.
         
         Returns zero if conversion was successful and complete. Otherwise
         returns E_UNEXPECTED_ENDOFINPUT indicating a partial multi-byte
         sequecne at the end, otherwise a negative number indicating any
         other error.
         
         Errors:
         E_TRAIL_EXPECTED:           trail byte expected or trail byte yields an unconvertable offset.
         E_UNEXPECTED_ENDOFINPUT:    unexpected and of input
         E_INVALID_START_BYTE:       invalid start byte
         E_INVALID_CODE_POINT:       detected surrogate or code point is invalid Unicode
         
         
         According the "Constraints on Conversion process" in the Unicode
         Standard v6, this algorithm conforms to the concept of "maximal sub-
         part".
         
         Safe version:
         
         - checks for EOF
         - checks for valid UTF input
         
         
         Unsafe version:
         
         The Unicode sequence shall be well-formed, otherwise the result is
         undefined.
         
         - does not check for valid start byte
         - does not check for valid trail byte
         - does not check for valid number of trail bytes.
         - does not ensure that first equals last on exit
         - may crash if input is not well-formed.
         */
        template <typename InIteratorT, typename OutIteratorT>
        int
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                          == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            

            code_point_t code_point[1];
            typedef typename internal::utf8_to_codepoint_actions<code_point_t*> actions_t;
            int result = 0;
            while (first != last and result == 0) {
                code_point_t* cp = code_point;
                result = parser_.parse(first, last, actions_t(), cp);
                if (result == 0) {
                    *dest++ = '\\';
                    *dest++ = 'u';
                    if (code_point[0] <= 0xFFFF) {
                        *dest++ = __hex_table[((code_point[0] >> 12) & 0x000F)];
                        *dest++ = __hex_table[((code_point[0] >> 8) & 0x000F)];
                        *dest++ = __hex_table[((code_point[0] >> 4) & 0x000F)];
                        *dest++ = __hex_table[((code_point[0]) & 0x000F)];
                    }
                    else {
                        uint16_t x = utf16_get_lead(code_point[0]);
                        *dest++ = '\\';
                        *dest++ = 'u';
                        *dest++ = __hex_table[((x >> 12) & 0x000F) ];
                        *dest++ = __hex_table[((x >> 8) & 0x000F)];
                        *dest++ = __hex_table[((x >> 4) & 0x000F)];
                        *dest++ = __hex_table[((x) & 0x000F)];
                        
                        x = utf16_get_trail(code_point[0]);
                        *dest++ = '\\';
                        *dest++ = 'u';
                        *dest++ = __hex_table[((x >> 12) & 0x000F) ];
                        *dest++ = __hex_table[((x >> 8) & 0x000F)];
                        *dest++ = __hex_table[((x >> 4) & 0x000F)];
                        *dest++ = __hex_table[((x) & 0x000F)];
                    }
                }
            }
            return result;
        }
        
    };  // class converter UTF-8 to Escaped Unicode Sequence
    
}}  // namespace json::unicode



#pragma mark - Class Template Specializations for UTF-16
namespace json { namespace unicode {
    
#pragma mark Struct converter  <UTF-16 to Unicode codepoint>
    //
    // struct converter <UTF-16 to code point>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful, ParseOne,
        typename std::enable_if<
            std::is_base_of<UTF_16_encoding_tag, FromEncodingT>::value
            and std::is_same<code_point_t, ToEncodingT>::value
        >::type
    >
    {        
        // Upgrade FromEncoding to include endianness if required:
        typedef typename add_endianness<FromEncodingT>::type            from_encoding_t;        
        typedef typename encoding_traits<from_encoding_t>::endian_tag   from_endian_t;
        typedef typename host_endianness::type                          to_endian_t;
        
        
        typedef internal::utf16_parser<
            from_endian_t,
            internal::utf16_validation_traits<Validation>::no_check_input_range, 
            internal::utf16_validation_traits<Validation>::no_check_trails,
            Stateful, ParseOne
        >                                                       parser_type;
        
    public:
        typedef typename parser_type::mb_state_type             mb_state_type;
        
    private:
        parser_type parser_;
        
    public:
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        

        mb_state_type state() const { return parser_.state(); }

        void clear() { parser_.clear(); }
        
        /**
        Convert the UTF-16 sequence starting at first to an Unicode code point.
        Endianness may be explicitly specified, or if not, the Encoding will be
        "upgraded" to an Encoding including host endianness.
        Unsafe version:
        - does not check for iterating past last
        - does not check for valid surrogate pairs.
        - does not check if first is a valid start byte for UTF-16
        - does not check for noncharacters

        Returns the number of consumed code units or a negative number 
        indicating an error.
        unsafe
        */        
        template <typename InIteratorT, typename OutIteratorT>
        int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            typedef typename internal::utf16_to_codepoint_actions<OutIteratorT> actions_t;
            
            int result = parser_.parse(first, last, actions_t(), dest);
            return result;
        }
        
    };

    
    
#pragma mark Struct converter  <Unicode codepoint to UTF-16>
    //
    // struct converter <code point to UTF-16>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful::No, ParseOne,
        typename std::enable_if<
            std::is_same<code_point_t, FromEncodingT>::value
            and std::is_base_of<UTF_16_encoding_tag, ToEncodingT>::value
        >::type
    >
    {
        // Upgrade Encoding to include endianness if required:
        typedef typename host_endianness::type                      host_endian_t;
        typedef typename add_endianness<ToEncodingT>::type          to_encoding_t;        
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
    
        void clear() {}
        
        // Convert a sequence of Unicode code point using UTF-16 encoding and 
        // copy the result into the sequence starting at dest.  
        // The output sequence must be capable to hold the number of generated
        // code units.
        //
        // Returns zero on success, or a negative number indicating the kind of
        // error.
        // 
        // If the encoding's endianness does not equal the host endianness a swap 
        // will automatically applied to the resulting code points.
        //
        // 
        // Errors:
        //  E_INVALID_CODE_POINT:   Surrogate or out of Unicode code space.
        //
        template <typename InIteratorT, typename OutIteratorT>
        static int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(code_point_t), "");
            
            // encode the unicode character to a UTF-16 code unit, or possibly
            // to a surrogate pair (two code units).
            // utf16_encoded_length() returns zero for surrogates and invalid Unicode
            // code points above U+10FFFF.
            uint16_t tmp;
            while (ParseOne or first != last) {
                code_point_t cp = *first;
                int len = utf16_encoded_length(cp);
                switch (len) {
                    case 1: 
                        tmp = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(cp));
                        *dest++ = tmp;
                        break;
                    case 2:
                        *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_lead(cp)));
                        *dest++ = byte_swap<host_endian_t, to_endian_t>(static_cast<uint16_t>(utf16_get_trail(cp)));
                        break;
                    default: 
                        return E_INVALID_CODE_POINT;  // surrogate or code_point > U+10FFFF
                }
                ++first;
                if (ParseOne) {
                    return 0;
                }
            }
            return 0;
        }
        
    };
    
    
#pragma mark Struct converter  <UTF-16 to any UTF>
    //
    // struct converter <UTF-16 to UTF-xx>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful, ParseOne,
        typename std::enable_if<
            std::is_base_of<UTF_16_encoding_tag, FromEncodingT>::value
            and internal::IsBaseAndDerived<utf_encoding_tag, ToEncodingT>::value
        >::type
    >
    {        
        // Upgrade FromEncoding and ToEncodingT to include endianness if required:
        typedef typename add_endianness<FromEncodingT>::type            from_encoding_t;
        typedef typename add_endianness<ToEncodingT>::type              to_encoding_t;        
        typedef typename encoding_traits<from_encoding_t>::endian_tag   from_endian_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag     to_endian_t;
        
        
        typedef internal::utf16_parser<
            from_endian_t,
            internal::utf16_validation_traits<Validation>::no_check_input_range, 
            internal::utf16_validation_traits<Validation>::no_check_trails,
            Stateful, ParseOne
        >                                                               parser_type;
        
    public:
        typedef typename parser_type::mb_state_type                     mb_state_type;
        
    private:
        parser_type parser_;
        
    public:        
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        
        
        mb_state_type state() const { return parser_.state(); }

        void clear() { parser_.clear(); }
        

        /**        
         Convert the Unicode sequence in 'from_encoding_t' into an Unicode 
         sequence in 'to_encoding_t' encoding form.
         
         Validation checks are performed according template parameter Validation.
         
         Unsafe version:
         - does not check for iterating past last
         - does not check for valid surrogate pairs.
         - does not check if first is a valid start byte for UTF-16
         - does not check for noncharacters
         
         Returns zero on success, otherwise a negative number indicating an error.
        */        
        template <typename InIteratorT, typename OutIteratorT>
        int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            typedef typename internal::utf16_to_utf_actions<OutIteratorT, to_encoding_t> actions_t;
            
            int result = parser_.parse(first, last, actions_t(), dest);
            return result;
        }
        
    };
    
    
#pragma mark Struct converter <UTF-16 to Escaped Unicode Sequence>
    //
    // struct converter
    //
    template <typename FromEncodingT, Validation::Flags Validation, Stateful::Option Stateful, ParseOne::Option ParseOne>
    struct converter<FromEncodingT, escaped_unicode_encoding_t, Validation, Stateful, ParseOne,
    typename std::enable_if<
            std::is_same<UTF_16_encoding_tag, FromEncodingT>::value
        >::type
    >
    {
        typedef typename add_endianness<FromEncodingT>::type            from_encoding_t;
        typedef typename encoding_traits<from_encoding_t>::endian_tag   from_endian_t;
        typedef UTF_8_encoding_tag                                      to_encoding_t;
        
        typedef internal::utf16_parser<
            from_endian_t,
            internal::utf16_validation_traits<Validation>::no_check_input_range,
            internal::utf16_validation_traits<Validation>::no_check_trails,
            Stateful, true/*parse one*/
        >                                                           parser_type;
        
        
    public:
        typedef typename parser_type::mb_state_type                 mb_state_type;
        
    private:
        parser_type parser_;
        
    public:
        converter() noexcept {}
        converter(mb_state_type const& state) noexcept : parser_(state) {}
        
        
        mb_state_type state() const { return parser_.state(); }
        
        void clear() { parser_.clear(); }
        
#pragma mark UTF-16 sequence to Escaped Unicode sequence
        /**
         Convert a sequence of Unicode characters in encoding form UTF-16
         to a corresponding sequence of Escapded Unicode sequences in
         UTF-8 and write it into output iterator 'dest'.
         The converter keeps state information required to parse partial
         multibyte sequences.
         
         If the converter starts with an initial input sequence, the state of
         the converter shall be cleared. The state is cleared after construction
         or after clear_state() has been called.
         
         If the sequence stops at a partial multibyte sequence - which was
         valid up to that point, the function returns E_UNEXPECTED_ENDOFINPUT
         and the conversion state is set such that a subsequent call of
         convert() can be proceed with the continuing sequence starting at
         the offset immediately following the last byte. The sequences can be
         divided at any code unit boundary.
         
         Returns zero if conversion was successful and complete. Otherwise
         returns E_UNEXPECTED_ENDOFINPUT indicating a partial multi-byte
         sequecne at the end, otherwise a negative number indicating any
         other error.
         
         Errors:
         E_TRAIL_EXPECTED:           trail byte expected or trail byte yields an unconvertable offset.
         E_UNEXPECTED_ENDOFINPUT:    unexpected and of input
         E_INVALID_START_BYTE:       invalid start byte
         E_INVALID_CODE_POINT:       detected surrogate or code point is invalid Unicode
         
         
         According the "Constraints on Conversion process" in the Unicode
         Standard v6, this algorithm conforms to the concept of "maximal sub-
         part".
         
         Safe version:
         
         - checks for EOF
         - checks for valid UTF input
         
         
         Unsafe version:
         
         The Unicode sequence shall be well-formed, otherwise the result is
         undefined.
         
         - does not check for valid start byte
         - does not check for valid trail byte
         - does not check for valid number of trail bytes.
         - does not ensure that first equals last on exit
         - may crash if input is not well-formed.
         */
        template <typename InIteratorT, typename OutIteratorT>
        int
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                          == sizeof(typename encoding_traits<FromEncodingT>::code_unit_type), "");
            
            code_point_t code_point;
            code_point_t* cp = &code_point;
            typedef typename internal::utf16_to_codepoint_actions<code_point_t*> actions_t;
            int result = 0;
            while (first != last and result == 0) {
                result = parser_.parse(first, last, actions_t(), cp);
                if (result == 0) {
                    *dest++ = '\\';
                    *dest++ = 'u';
                    if (code_point <= 0xFFFF) {
                        *dest++ = __hex_table[((code_point >> 12) & 0x000F) ];
                        *dest++ = __hex_table[((code_point >> 8) & 0x000F)];
                        *dest++ = __hex_table[((code_point >> 4) & 0x000F)];
                        *dest++ = __hex_table[((code_point >> 0) & 0x000F)];
                    }
                    else {
                        uint16_t x = utf16_get_lead(code_point);
                        *dest++ = '\\';
                        *dest++ = 'u';
                        *dest++ = __hex_table[((x >> 12) & 0x000F) ];
                        *dest++ = __hex_table[((x >> 8) & 0x000F)];
                        *dest++ = __hex_table[((x >> 4) & 0x000F)];
                        *dest++ = __hex_table[((x) & 0x000F)];
                        
                        x = utf16_get_trail(code_point);
                        *dest++ = '\\';
                        *dest++ = 'u';
                        *dest++ = __hex_table[((x >> 12) & 0x000F) ];
                        *dest++ = __hex_table[((x >> 8) & 0x000F)];
                        *dest++ = __hex_table[((x >> 4) & 0x000F)];
                        *dest++ = __hex_table[((x) & 0x000F)];
                    }
                }
            }
            return result;
        }
        
    };  // class converter UTF-8 to Escaped Unicode Sequence
    
}}    // namespace json::unicode



#pragma mark - Class Template Specializations for UTF-32
namespace json { namespace unicode {
    
#pragma mark Struct converter  <UTF-32 to Unicode codepoint>
    //
    // struct converter <UTF-32 to code point>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful::No, ParseOne,
        typename std::enable_if<
            std::is_base_of<UTF_32_encoding_tag, FromEncodingT>::value
            and std::is_same<code_point_t, ToEncodingT>::value
        >::type
    >
    {        
        // Upgrade FromEncoding to include endianness if required:
        typedef typename add_endianness<FromEncodingT>::type            from_encoding_t;        
        typedef typename encoding_traits<from_encoding_t>::endian_tag   from_endian_t;
        typedef typename host_endianness::type                          to_endian_t;
        
        
        typedef internal::utf32_parser<
            from_endian_t,
            internal::utf32_validation_traits<Validation>::no_check_valid_unicode,
            ParseOne
        >                                                       parser_type;
         
        
    public:
        typedef internal::mb_state<UTF_32_encoding_tag>         mb_state_type;
        
    private:
        parser_type parser_;
        
    public:

        converter() noexcept {}
        converter(mb_state_type const&) noexcept {}
        
        mb_state_type state() const { return mb_state_type(); }        

        void clear() {}

        
        template <typename InIteratorT, typename OutIteratorT>
        int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<from_encoding_t>::code_unit_type), "");

            typedef internal::utf32_to_codepoint_actions<OutIteratorT> actions_t;
            
            int result =  parser_.parse(first, last, actions_t(), dest);
            return result;
        }
    };    
    

    
#pragma mark Struct converter  <Unicode codepoint to UTF-32>
    //
    // struct converter <code point to UTF-32>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, ParseOne::Option ParseOne>
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful::No, ParseOne, 
        typename std::enable_if<
            std::is_same<code_point_t, FromEncodingT>::value
            and std::is_base_of<UTF_32_encoding_tag, ToEncodingT>::value
        >::type
    >
    {
        // Upgrade Encoding to include endianness if required:
        typedef typename host_endianness::type                      from_endian_t;
        typedef typename add_endianness<ToEncodingT>::type          to_encoding_t;        
        typedef typename encoding_traits<to_encoding_t>::endian_tag to_endian_t;
        
        void clear() {}
        
        
        // Convert sequence of Unicode code points using UTF-32 encoding and 
        // copy the result into the sequence starting at dest.  
        //
        // Returns zero on success, otherwise a negative mumber indicating the
        // kind of error.
        // 
        // Errors:
        //  E_INVALID_CODE_POINT:   Surrogate or out of Unicode code space.
        //
        template <typename InIteratorT, typename OutIteratorT>
        static int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(code_point_t), "");
            
            while (ParseOne or first != last) {
                if (internal::utf32_validation_traits<Validation>::no_check_valid_unicode 
                    or isUnicodeScalarValue(*first)) {
                    *dest++ = byte_swap<from_endian_t, to_endian_t>(*first++);
                    if (ParseOne) {
                        return 0;
                    }                    
                } else {
                    return E_INVALID_CODE_POINT;
                }
            }            
            return 0;
        }
        
    };
    
    
#pragma mark Struct converter  <UTF-32 to any UTF>
    //
    // struct converter <UTF-32 to UTF-XX>
    //
    template <typename FromEncodingT, typename ToEncodingT, 
        Validation::Flags Validation, ParseOne::Option ParseOne
    >
    struct converter<FromEncodingT, ToEncodingT, Validation, Stateful::No, ParseOne,
        typename std::enable_if<
            std::is_base_of<UTF_32_encoding_tag, FromEncodingT>::value
            and internal::IsBaseAndDerived<utf_encoding_tag, ToEncodingT>::value
        >::type
    >
    {        
        // Upgrade FromEncoding and ToEncodingT to include endianness if required:
        typedef typename add_endianness<FromEncodingT>::type            from_encoding_t;        
        typedef typename add_endianness<ToEncodingT>::type              to_encoding_t;        
        typedef typename encoding_traits<from_encoding_t>::endian_tag   from_endian_t;
        typedef typename encoding_traits<to_encoding_t>::endian_tag     to_endian_t;
        
        
        typedef internal::utf32_parser<
            from_endian_t,
            internal::utf32_validation_traits<Validation>::no_check_valid_unicode,
            ParseOne
        >                                                       parser_type;
        
    public:
        typedef internal::mb_state<UTF_32_encoding_tag>         mb_state_type;        
        
    private:
        parser_type parser_;
    public:
        
        converter() noexcept {}
        converter(mb_state_type const&) noexcept {}
        
        mb_state_type state() const { return mb_state_type(); }
        void clear() {}
        
        template <typename InIteratorT, typename OutIteratorT>
        int 
#if defined (TEST_CONVERT_FORCE_INLINE)
        __attribute__((always_inline))
#endif                            
        convert(InIteratorT& first, InIteratorT last, OutIteratorT& dest)
        {
            static_assert(sizeof(typename boost::iterator_value<InIteratorT>::type)
                                == sizeof(typename encoding_traits<from_encoding_t>::code_unit_type), "");
            
            typedef internal::utf32_to_utf_actions<OutIteratorT, to_encoding_t> actions_t;
            
            int result =  parser_.parse(first, last, actions_t(), dest);
            return result;
        }
    };    
    


}}



    



#endif // JSON_UNICODE_UNICODE_CONVERTER_HPP
