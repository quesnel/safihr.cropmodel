/*
 * Copyright (C) 2013 INRA
 *
 * Gauthier Quesnel <gauthier.quesnel@toulouse.inra.fr>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SAFIHR_MODEL_AI_HPP
#define SAFIHR_MODEL_AI_HPP

#include <boost/date_time.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/algorithm/string.hpp>
#include <exception>
#include <vle/utils/i18n.hpp>
#include <vle/utils/DateTime.hpp>
#include "Global.hpp"

namespace safihr {

struct ai_open_failure : std::runtime_error
{
    explicit ai_open_failure(const std::string &filepath)
        : std::runtime_error(
            (vle::fmt("AI: can not open `%1%'") % filepath).str())
    {}
};

struct ai_format_failure : std::runtime_error
{
    explicit ai_format_failure(uint line)
        : std::runtime_error(
            (vle::fmt("AI: fail to read line `%1%'") % line).str())
    {}
};

struct ai_internal_failure : std::runtime_error
{
    explicit ai_internal_failure(vle::devs::Time first, vle::devs::Time second)
        : std::runtime_error(
            (vle::fmt("AI: simulation begin at `%1%', AI data at `%2%'") % first
             % second).str())
    {}

    explicit ai_internal_failure(vle::devs::Time first, vle::devs::Time second,
                                 vle::devs::Time duration)
        : std::runtime_error(
            (vle::fmt("AI: `%1%' - `%2%' != `%3%'") % first % second %
             duration).str())
    {}
};


/**
 * Convert a date into julian day date in vle::devs::Time
 * type. @attention The date must be in format: "dd/mm/yy".
 *
 * @param date A data in the format "dd/mm/yy".
 *
 * @return A julian day date.
 */
inline vle::devs::Time ai_convert_date(std::string date)
{
    namespace ba = boost::algorithm;

    ba::split_iterator <std::string::iterator> i, e;
    i = ba::make_split_iterator(date,
                                ba::token_finder(
                                    ba::is_any_of("/"),
                                    ba::token_compress_on));

    int day = safihr::stoi(boost::copy_range <std::string>(*i++));
    int month = safihr::stoi(boost::copy_range <std::string>(*i++));
    int year = safihr::stoi(boost::copy_range <std::string>(*i++));

    boost::gregorian::date d(year, month, day);

    return boost::numeric_cast <vle::devs::Time>(d.julian_day());
}

}

#endif
