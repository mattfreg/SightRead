#include <charconv>
#include <optional>

#include <boost/spirit/home/x3.hpp>

#include "sightread/detail/chart.hpp"
#include "sightread/songparts.hpp"

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

SightRead::Detail::NoteEvent convert_line_to_note(std::string_view line)
{
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::ascii::space;

    SightRead::Detail::NoteEvent event;

    auto set_pos = [&](auto& ctx) { event.position = _attr(ctx); };
    auto set_fret = [&](auto& ctx) { event.fret = _attr(ctx); };
    auto set_length = [&](auto& ctx) { event.length = _attr(ctx); };

    auto first = line.cbegin();
    auto last = line.cend();

    bool r = phrase_parse(
        first, last,
        (int_[set_pos] >> '=' >> 'N' >> int_[set_fret] >> int_[set_length]),
        space);

    if (!r || first != last) {
        throw SightRead::ParseError("Bad note event");
    }

    return event;
}

SightRead::Detail::SpecialEvent convert_line_to_special(std::string_view line)
{
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::ascii::space;

    SightRead::Detail::SpecialEvent event;

    auto set_pos = [&](auto& ctx) { event.position = _attr(ctx); };
    auto set_key = [&](auto& ctx) { event.key = _attr(ctx); };
    auto set_length = [&](auto& ctx) { event.length = _attr(ctx); };

    auto first = line.cbegin();
    auto last = line.cend();

    bool r = phrase_parse(
        first, last,
        (int_[set_pos] >> '=' >> 'S' >> int_[set_key] >> int_[set_length]),
        space);

    if (!r || first != last) {
        throw SightRead::ParseError("Bad special event");
    }

    return event;
}

SightRead::Detail::BpmEvent convert_line_to_bpm(std::string_view line)
{
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::ascii::space;

    SightRead::Detail::BpmEvent event;

    auto set_pos = [&](auto& ctx) { event.position = _attr(ctx); };
    auto set_bpm = [&](auto& ctx) { event.bpm = _attr(ctx); };

    auto first = line.cbegin();
    auto last = line.cend();

    bool r = phrase_parse(
        first, last, (int_[set_pos] >> '=' >> 'B' >> int_[set_bpm]), space);

    if (!r || first != last) {
        throw SightRead::ParseError("Bad BPM event");
    }

    return event;
}

SightRead::Detail::TimeSigEvent convert_line_to_timesig(std::string_view line)
{
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::ascii::space;

    SightRead::Detail::TimeSigEvent event;
    event.denominator = 2;

    auto set_pos = [&](auto& ctx) { event.position = _attr(ctx); };
    auto set_numer = [&](auto& ctx) { event.numerator = _attr(ctx); };
    auto set_denom = [&](auto& ctx) { event.denominator = _attr(ctx); };

    auto first = line.cbegin();
    auto last = line.cend();

    bool r = phrase_parse(
        first, last,
        (int_[set_pos] >> '=' >> "TS" >> int_[set_numer] >> -int_[set_denom]),
        space);

    if (!r || first != last) {
        throw SightRead::ParseError("Bad TS event");
    }

    return event;
}

SightRead::Detail::Event convert_line_to_event(std::string_view line)
{
    using boost::spirit::x3::_attr;
    using boost::spirit::x3::char_;
    using boost::spirit::x3::int_;
    using boost::spirit::x3::lexeme;
    using boost::spirit::x3::phrase_parse;
    using boost::spirit::x3::ascii::space;

    SightRead::Detail::Event event;

    auto set_pos = [&](auto& ctx) { event.position = _attr(ctx); };
    auto set_data = [&](auto& ctx) { event.data = _attr(ctx); };

    auto first = line.cbegin();
    auto last = line.cend();

    bool r = phrase_parse(
        first, last,
        (int_[set_pos] >> '=' >> 'E' >> lexeme[*~char_(' ')][set_data]), space);

    if (!r || first != last) {
        throw SightRead::ParseError("Bad event");
    }

    return event;
}

SightRead::Detail::ChartSection read_section(std::string_view& input)
{
    SightRead::Detail::ChartSection section;
    section.name = strip_square_brackets(break_off_newline(input));

    if (break_off_newline(input) != "{") {
        throw SightRead::ParseError("Section does not open with {");
    }

    while (true) {
        const auto next_line = break_off_newline(input);
        if (next_line == "}") {
            break;
        }
        const auto separated_line = split_by_space(next_line);
        if (separated_line.size() < 3) {
            throw SightRead::ParseError("Line incomplete");
        }
        const auto key = separated_line[0];
        const auto key_val = string_view_to_int(key);
        if (key_val.has_value()) {
            if (separated_line[2] == "N") {
                section.note_events.push_back(convert_line_to_note(next_line));
            } else if (separated_line[2] == "S") {
                section.special_events.push_back(
                    convert_line_to_special(next_line));
            } else if (separated_line[2] == "B") {
                section.bpm_events.push_back(convert_line_to_bpm(next_line));
            } else if (separated_line[2] == "TS") {
                section.ts_events.push_back(convert_line_to_timesig(next_line));
            } else if (separated_line[2] == "E") {
                section.events.push_back(convert_line_to_event(next_line));
            }
        } else {
            std::string value {separated_line[2]};
            for (auto i = 3U; i < separated_line.size(); ++i) {
                value.append(separated_line[i]);
            }
            section.key_value_pairs[std::string(key)] = value;
        }
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
