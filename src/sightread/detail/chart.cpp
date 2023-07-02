#include <charconv>
#include <optional>

#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>

#include "sightread/detail/chart.hpp"
#include "sightread/songparts.hpp"

namespace {
namespace grammar {
    namespace dsl = lexy::dsl;

    struct note_event {
        static constexpr auto whitespace = dsl::ascii::blank;

        static constexpr auto rule = dsl::integer<int> + dsl::lit_c<'='>
            + dsl::lit_c<'N'> + dsl::times<2>(dsl::integer<int>) + dsl::eof;
        static constexpr auto value
            = lexy::construct<SightRead::Detail::NoteEvent>;
    };

    struct special_event {
        static constexpr auto whitespace = dsl::ascii::blank;

        static constexpr auto rule = dsl::integer<int> + dsl::lit_c<'='>
            + dsl::lit_c<'S'> + dsl::times<2>(dsl::integer<int>) + dsl::eof;
        static constexpr auto value
            = lexy::construct<SightRead::Detail::SpecialEvent>;
    };

    struct bpm_event {
        static constexpr auto whitespace = dsl::ascii::blank;

        static constexpr auto rule = dsl::integer<int> + dsl::lit_c<'='>
            + dsl::lit_c<'B'> + dsl::integer<int> + dsl::eof;
        static constexpr auto value
            = lexy::construct<SightRead::Detail::BpmEvent>;
    };

    struct ts_event {
        static constexpr auto whitespace = dsl::ascii::blank;

        static constexpr auto rule = dsl::integer<int> + dsl::lit_c<'='>
            + LEXY_LIT("TS") + dsl::integer<int>
            + dsl::try_(dsl::integer<int>, dsl::nullopt) + dsl::eof;
        static constexpr auto value
            = lexy::bind(lexy::construct<SightRead::Detail::TimeSigEvent>,
                         lexy::_1, lexy::_2, lexy::_3 or 2);
    };

    struct rest_of_event {
        static constexpr auto rule
            = dsl::identifier(dsl::ascii::alnum / dsl::lit_c<'_'>);
        static constexpr auto value = lexy::as_string<std::string>;
    };

    struct event {
        static constexpr auto whitespace = dsl::ascii::blank;

        static constexpr auto rule = dsl::integer<int> + dsl::lit_c<'='>
            + dsl::lit_c<'E'> + dsl::p<rest_of_event> + dsl::eof;
        static constexpr auto value = lexy::construct<SightRead::Detail::Event>;
    };
}

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
    std::string error;

    const auto literal = lexy::string_input(line);
    const auto result = lexy::parse<grammar::note_event>(
        literal, lexy_ext::report_error.to(std::back_inserter(error)));

    if (!result.has_value()) {
        throw SightRead::ParseError(error);
    }

    return result.value();
}

SightRead::Detail::SpecialEvent convert_line_to_special(std::string_view line)
{
    std::string error;

    const auto literal = lexy::string_input(line);
    const auto result = lexy::parse<grammar::special_event>(
        literal, lexy_ext::report_error.to(std::back_inserter(error)));

    if (!result.has_value()) {
        throw SightRead::ParseError(error);
    }

    return result.value();
}

SightRead::Detail::BpmEvent convert_line_to_bpm(std::string_view line)
{
    std::string error;

    const auto literal = lexy::string_input(line);
    const auto result = lexy::parse<grammar::bpm_event>(
        literal, lexy_ext::report_error.to(std::back_inserter(error)));

    if (!result.has_value()) {
        throw SightRead::ParseError(error);
    }

    return result.value();
}

SightRead::Detail::TimeSigEvent convert_line_to_timesig(std::string_view line)
{
    std::string error;

    const auto literal = lexy::string_input(line);
    const auto result = lexy::parse<grammar::ts_event>(
        literal, lexy_ext::report_error.to(std::back_inserter(error)));

    if (!result.has_value()) {
        throw SightRead::ParseError(error);
    }

    return result.value();
}

SightRead::Detail::Event convert_line_to_event(std::string_view line)
{
    std::string error;

    const auto literal = lexy::string_input(line);
    const auto result = lexy::parse<grammar::event>(
        literal, lexy_ext::report_error.to(std::back_inserter(error)));

    if (!result.has_value()) {
        throw SightRead::ParseError(error);
    }

    return result.value();
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
