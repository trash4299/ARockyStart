/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <rocky/Math.h>
#include <rocky/Status.h>
#include <rocky/Log.h>

#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <locale>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <ctype.h>
#include <functional>
#include <chrono>
#include <thread>

class TiXmlDocument;

namespace ROCKY_NAMESPACE { namespace util
{
    extern ROCKY_EXPORT const std::string EMPTY_STRING;

    using StringVector = std::vector<std::string>;
    using StringSet = std::set<std::string>;
    using StringTable = std::unordered_map<std::string, std::string>;

    /** Replaces all the instances of "pattern" with "replacement" in "in_out" */
    extern ROCKY_EXPORT std::string& replace_in_place(
        std::string&       in_out,
        const std::string& pattern,
        const std::string& replacement );

    /** Replaces all the instances of "pattern" with "replacement" in "in_out" (case-insensitive) */
    extern ROCKY_EXPORT std::string& replace_in_place_case_insensitive(
        std::string&       in_out,
        const std::string& pattern,
        const std::string& replacement );

    //! Trims whitespace from the ends of a string.
    extern ROCKY_EXPORT std::string trim(const std::string& in);

    //! Trims whitespace from the ends of a string; in-place modification on the string to reduce string copies.
    extern ROCKY_EXPORT void trimInPlace(std::string& str);

    //! Removes leading and trailing whitespace, and replaces all other
    //! whitespace with single spaces
    extern ROCKY_EXPORT std::string trimAndCompress(const std::string& in);

    //! True is "ref" starts with "pattern"
    extern ROCKY_EXPORT bool startsWith(
        const std::string& ref,
        const std::string& pattern,
        bool               caseSensitive =true,
        const std::locale& locale        =std::locale() );

    //! True is "ref" ends with "pattern"
    extern ROCKY_EXPORT bool endsWith(
        const std::string& ref,
        const std::string& pattern,
        bool               caseSensitive =true,
        const std::locale& locale        =std::locale() );

    //! Case-insensitive compare
    extern ROCKY_EXPORT bool ciEquals(
        const std::string& lhs,
        const std::string& rhs,
        const std::locale& local = std::locale() );

    //! Case-insensitive STL comparator
    struct ROCKY_EXPORT CIStringComp {
        bool operator()(const std::string& lhs, const std::string& rhs) const;
    };


    extern ROCKY_EXPORT std::string joinStrings( const StringVector& input, char delim );

    /** Returns a lower-case version of the input string. */
    extern ROCKY_EXPORT std::string toLower( const std::string& input );

    /** Makes a valid filename out of a string */
    extern ROCKY_EXPORT std::string toLegalFileName( const std::string& input, bool allowSubdir=false, const char* replacementChar=NULL);

    /** Generates a hashed integer for a string (poor man's MD5) */
    extern ROCKY_EXPORT unsigned hashString( const std::string& input );

    /** Same as hashString but returns a string value. */
    extern ROCKY_EXPORT std::string hashToString(const std::string& input);

    //! Gets the total number of seconds formatted as H:M:S
    extern ROCKY_EXPORT std::string prettyPrintTime( double seconds );

    //! Gets a pretty printed version of the given size in MB.
    extern ROCKY_EXPORT std::string prettyPrintSize( double mb );  
    
    //! Extract the "i-th" token from a delimited string
    extern ROCKY_EXPORT std::string getToken(const std::string& input, unsigned i, const std::string& delims=",");

    extern ROCKY_EXPORT u8vec4 toColor(const std::string& str, const u8vec4& default_value);

    extern ROCKY_EXPORT std::string makeCacheKey(const std::string& key, const std::string& prefix);
    //------------------------------------------------------------------------
    // conversion templates

    // converts a string to primitive using serialization
    template<typename T> inline T
    as( const std::string& str, const T& default_value )
    {
        T temp = default_value;
        std::istringstream strin( str );
        if ( !strin.eof() ) strin >> temp;
        return temp;
    }

    // template specialization for integers (to handle hex)
#define AS_INT_DEC_OR_HEX(TYPE) \
    template<> inline TYPE \
    as< TYPE >(const std::string& str, const TYPE & dv) { \
        TYPE temp = dv; \
        std::istringstream strin( trim(str) ); \
        if ( !strin.eof() ) { \
            if ( str.length() >= 2 && str[0] == '0' && str[1] == 'x' ) { \
                strin.seekg( 2 ); \
                strin >> std::hex >> temp; \
            } \
            else { \
                strin >> temp; \
            } \
        } \
        return temp; \
    }

    AS_INT_DEC_OR_HEX(int)
    AS_INT_DEC_OR_HEX(unsigned)
    AS_INT_DEC_OR_HEX(short)
    AS_INT_DEC_OR_HEX(unsigned short)
    AS_INT_DEC_OR_HEX(long)
    AS_INT_DEC_OR_HEX(unsigned long)

    // template specialization for a bool
    template<> inline bool
    as<bool>( const std::string& str, const bool& default_value )
    {
        std::string temp = toLower(str);
        return
            temp == "true"  || temp == "yes" || temp == "on" ? true :
            temp == "false" || temp == "no" || temp == "off" ? false :
            default_value;
    }

    // template specialization for string
    template<> inline std::string
    as<std::string>( const std::string& str, const std::string& default_value )
    {
        return str;
    }

    // snips a substring and parses it.
    template<typename T> inline bool
    as(const std::string& in, unsigned start, unsigned len, T default_value)
    {
        std::string buf;
        std::copy( in.begin()+start, in.begin()+start+len, std::back_inserter(buf) );
        return as<T>(buf, default_value);
    }

    // converts a primitive to a string
    template<typename T> inline std::string
    toString(const T& value)
    {
        std::stringstream out;
		//out << std::setprecision(20) << std::fixed << value;
		out << std::setprecision(20) <<  value;
        std::string outStr;
        outStr = out.str();
        return outStr;
    }

    // template speciallization for a bool to print out "true" or "false"
    template<> inline std::string
    toString<bool>(const bool& value)
    {
        return value ? "true" : "false";
    }

    /**
     * Assembles and returns an inline string using a stream-like << operator.
     * Example:
     *     std::string str = Stringify() << "Hello, world " << variable;
     */
    struct Stringify
    {
        operator std::string () const
        {
            std::string result;
            result = buf.str();
            return result;
        }

        template<typename T>
        Stringify& operator << (const T& val) { buf << val; return (*this); }

        Stringify& operator << (const Stringify& val) { buf << (std::string)val; return (*this); }

    protected:
        std::stringstream buf;
    };

    using make_string = Stringify;

    template<> inline
    Stringify& Stringify::operator << <bool>(const bool& val) { buf << (val ? "true" : "false"); return (*this); }

    /**
     * Splits a string up into a vector of strings based on a set of
     * delimiters, quotes, and rules.
     */
    class ROCKY_EXPORT StringTokenizer
    {
    public:
        StringTokenizer( 
            const std::string& delims =" \t\r\n", 
            const std::string& quotes ="'\"" );

        StringTokenizer(
            const std::string& input,
            StringVector& output,
            const std::string& delims =" \t\r\n", 
            const std::string& quotes ="'\"",
            bool keepEmpties =true, 
            bool trimTokens =true);

        StringTokenizer(
            const std::string& input,
            StringTable& output,
            const std::string& delims = " \t\r\n",
            const std::string& keypairseparators = "=",
            const std::string& quotes = "'\"",
            bool keepEmpties = true,
            bool trimTokens = true);


        void tokenize( const std::string& input, StringVector& output ) const;

        bool& keepEmpties() { return _allowEmpties; }

        bool& trimTokens() { return _trimTokens; }

        void addDelim( char delim, bool keepAsToken =false );

        void addDelims( const std::string& delims, bool keepAsTokens =false );

        void addQuote( char delim, bool keepInToken =false );

        void addQuotes( const std::string& delims, bool keepInTokens =false );

    private:
        using TokenMap = std::unordered_map<char,bool>;
        TokenMap _delims;
        TokenMap _quotes;
        bool     _allowEmpties;
        bool     _trimTokens;
    };

#if 0
    extern ROCKY_EXPORT bool isURL(const std::string&);

    extern ROCKY_EXPORT bool isAbsolutePath(const std::string&);

    extern ROCKY_EXPORT bool isRelativePath(const std::string&);

    extern ROCKY_EXPORT std::string getAbsolutePath(const std::string&);
#endif

    /**
     * A filesystem Path that automatically normalizes
     * pathnames
     */
    class ROCKY_EXPORT Path : public std::filesystem::path
    {
    public:
        Path(const std::string& str) :
            std::filesystem::path(str) { normalize(); }

    private:
        void normalize();
    };


    /**
     * Tracks usage data by maintaining a sentry-blocked linked list.
     * Each time a use called "use" the corresponding record moves to
     * the right of the sentry marker. After a cycle you can call
     * collectTrash to process all users that did not call use() in the
     * that cycle, and dispose of them.
     */
    template<typename T>
    class SentryTracker
    {
    public:
        struct ListEntry
        {
            ListEntry(T data, void* token) : _data(data), _token(token) { }
            T _data;
            void* _token;
        };

        using List = std::list<ListEntry>;
        using ListIterator = typename List::iterator;
        using Token = ListIterator;

        SentryTracker()
        {
            reset();
        }

        ~SentryTracker()
        {
            for (auto& e : _list)
            {
                Token* te = static_cast<Token*>(e._token);
                if (te)
                    delete te;
            }
        }

        void reset()
        {
            for (auto& e : _list)
            {
                Token* te = static_cast<Token*>(e._token);
                if (te)
                    delete te;
            }
            _list.clear();
            _list.emplace_front(nullptr, nullptr); // the sentry marker
            _sentryptr = _list.begin();
        }

        List _list;
        ListIterator _sentryptr;

        inline void* use(const T& data, void* token)
        {
            // Find the tracker for this tile and update its timestamp
            if (token)
            {
                Token* ptr = static_cast<Token*>(token);

                // Move the tracker to the front of the list (ahead of the sentry).
                // Once a cull traversal is complete, all visited tiles will be
                // in front of the sentry, leaving all non-visited tiles behind it.
                _list.splice(_list.begin(), _list, *ptr);
                *ptr = _list.begin();
                return ptr;
            }
            else
            {
                // New entry:
                Token* ptr = new Token();
                _list.emplace_front(data, ptr); // ListEntry
                *ptr = _list.begin();
                return ptr;
            }
        }

        inline void flush(
            unsigned maxCount,
            std::function<bool(T& obj)> dispose)
        {
            // After cull, all visited tiles are in front of the sentry, and all
            // non-visited tiles are behind it. Start at the sentry position and
            // iterate over the non-visited tiles, checking them for deletion.
            ListIterator i = _sentryptr;
            ListIterator tmp;
            unsigned count = 0;

            for (++i; i != _list.end() && count < maxCount; ++i)
            {
                ListEntry& le = *i;

                bool disposed = true;

                // user disposal function
                if (dispose != nullptr)
                    disposed = dispose(le._data);

                if (disposed)
                {
                    // back up the iterator so we can safely erase the entry:
                    tmp = i;
                    --i;

                    // delete the token
                    delete static_cast<Token*>(le._token);

                    // remove it from the tracker list:
                    _list.erase(tmp);
                    ++count;
                }
            }

            // reset the sentry.
            _list.splice(_list.begin(), _list, _sentryptr);
            _sentryptr = _list.begin();
        }
    };

    struct scoped_chrono
    {
        std::string _me;
        std::chrono::time_point<std::chrono::steady_clock> _a;
        scoped_chrono(const std::string& me) : _me(me) {
            _a = std::chrono::steady_clock::now();
        }
        ~scoped_chrono() {
            auto b = std::chrono::steady_clock::now();
            auto d = (float)std::chrono::duration_cast<std::chrono::microseconds>(b - _a).count();
            rocky::Log::info() << std::this_thread::get_id() << " : " << _me << " = " << d << "us" << std::endl;
        }
    };
} }