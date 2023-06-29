#include <charconv>
#include <optional>

#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>

#include "sightread/detail/chart.hpp"
#include "sightread/songparts.hpp"

BOOST_FUSION_ADAPT_STRUCT(SightRead::Detail::NoteEvent, position, fret, length)
BOOST_FUSION_ADAPT_STRUCT(SightRead::Detail::SpecialEvent, position, key,
                          length)
BOOST_FUSION_ADAPT_STRUCT(SightRead::Detail::BpmEvent, position, bpm)
BOOST_FUSION_ADAPT_STRUCT(SightRead::Detail::TimeSigEvent, position, numerator,
                          denominator)
BOOST_FUSION_ADAPT_STRUCT(SightRead::Detail::Event, position, data)

namespace SightRead {
namespace x3 = boost::spirit::x3;

using x3::attr;
using x3::char_;
using x3::int_;
using x3::lexeme;
using x3::ascii::space;

const x3::rule<class note_event, SightRead::Detail::NoteEvent> note_event
    = "note_event";
const auto note_event_def = int_ >> '=' >> 'N' >> int_ >> int_;
BOOST_SPIRIT_DEFINE(note_event);

const x3::rule<class special_event, SightRead::Detail::SpecialEvent>
    special_event = "special_event";
const auto special_event_def = int_ >> '=' >> 'S' >> int_ >> int_;
BOOST_SPIRIT_DEFINE(special_event);

const x3::rule<class bpm_event, SightRead::Detail::BpmEvent> bpm_event
    = "bpm_event";
const auto bpm_event_def = int_ >> '=' >> 'B' >> int_;
BOOST_SPIRIT_DEFINE(bpm_event);

const x3::rule<class ts_event, SightRead::Detail::TimeSigEvent> ts_event
    = "ts_event";
const auto ts_event_def = int_ >> '=' >> "TS" >> int_ >> (int_ | attr(2));
BOOST_SPIRIT_DEFINE(ts_event);

const x3::rule<class general_event, SightRead::Detail::Event> general_event
    = "general_event";
const auto general_event_def = int_ >> '=' >> 'E' >> lexeme[*~char_(' ')];
BOOST_SPIRIT_DEFINE(general_event);

const x3::rule<class kv_pair, boost::fusion::vector<std::string, std::string>>
    kv_pair = "kv_pair";
const auto kv_pair_def = lexeme[*~char_(' ')] >> '=' >> lexeme[*char_];
BOOST_SPIRIT_DEFINE(kv_pair);
}

namespace {
std::string_view skip_whitespace(std::string_view input)
{
    const auto first_non_ws_location = input.find_first_not_of(" \f\n\r\t\v");
    input.remove_prefix(std::min(first_non_ws_location, input.size()));
    return input;
}

std::string_view break_off_newline(std::string_view& input)
{
    if (input.empty()) {
        throw SightRead::ParseError("No lines left");
    }

    const auto newline_location
        = std::min(input.find('\n'), input.find("\r\n"));
    if (newline_location == std::string_view::npos) {
        const auto line = input;
        input.remove_prefix(input.size());
        return line;
    }

    const auto line = input.substr(0, newline_location);
    input.remove_prefix(newline_location);
    input = skip_whitespace(input);
    return line;
}

std::string_view strip_square_brackets(std::string_view input)
{
    if (input.empty()) {
        throw SightRead::ParseError("Header string empty");
    }
    return input.substr(1, input.size() - 2);
}

// Convert a string_view to an int. If there are any problems with the input,
// this function returns std::nullopt.
std::optional<int> string_view_to_int(std::string_view input)
{
    int result = 0;
    const char* last = input.data() + input.size();
    auto [p, ec] = std::from_chars(input.data(), last, result);
    if ((ec != std::errc()) || (p != last)) {
        return std::nullopt;
    }
    return result;
}

// Split input by space characters, similar to .Split(' ') in C#. Note that
// the lifetime of the string_views in the output is the same as that of the
// input.
std::vector<std::string_view> split_by_space(std::string_view input)
{
    std::vector<std::string_view> substrings;

    while (true) {
        const auto space_location = input.find(' ');
        if (space_location == std::string_view::npos) {
            break;
        }
        substrings.push_back(input.substr(0, space_location));
        input.remove_prefix(space_location + 1);
    }

    substrings.push_back(input);
    return substrings;
}

SightRead::Detail::ChartSection read_section(std::string_view& input)
{
    using boost::spirit::x3::ascii::space;
    using SightRead::bpm_event;
    using SightRead::general_event;
    using SightRead::kv_pair;
    using SightRead::note_event;
    using SightRead::special_event;
    using SightRead::ts_event;

    SightRead::Detail::ChartSection section;
    section.name = strip_square_brackets(break_off_newline(input));

    auto add_note
        = [&](auto& ctx) { section.note_events.push_back(_attr(ctx)); };
    auto add_special
        = [&](auto& ctx) { section.special_events.push_back(_attr(ctx)); };
    auto add_bpm = [&](auto& ctx) { section.bpm_events.push_back(_attr(ctx)); };
    auto add_ts = [&](auto& ctx) { section.ts_events.push_back(_attr(ctx)); };
    auto add_event = [&](auto& ctx) { section.events.push_back(_attr(ctx)); };
    auto add_kv_pair = [&](auto& ctx) {
        const auto& pair = _attr(ctx);
        section.key_value_pairs.insert_or_assign(at_c<0>(pair), at_c<1>(pair));
    };

    if (break_off_newline(input) != "{") {
        throw SightRead::ParseError("Section does not open with {");
    }

    while (true) {
        const auto next_line = break_off_newline(input);
        if (next_line == "}") {
            break;
        }
        const auto first = next_line.cbegin();
        const auto last = next_line.cend();

        phrase_parse(first, last,
                     note_event[add_note] | special_event[add_special]
                         | bpm_event[add_bpm] | ts_event[add_ts]
                         | general_event[add_event] | kv_pair[add_kv_pair],
                     space);
    }

    return section;
}
}

SightRead::Detail::Chart SightRead::Detail::parse_chart(std::string_view data)
{
    SightRead::Detail::Chart chart;

    while (!data.empty()) {
        chart.sections.push_back(read_section(data));
    }

    return chart;
}
