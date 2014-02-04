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

#ifndef SAFIHR_MODEL_GLOBAL_HPP
#define SAFIHR_MODEL_GLOBAL_HPP

#include <exception>
#include <string>
#include <vle/utils/i18n.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/cast.hpp>

namespace safihr {

struct global_unknown_model_status : std::runtime_error
{
    global_unknown_model_status()
        : std::runtime_error("Unknown model status")
    {}
};

struct global_conversion_error : std::runtime_error
{
    global_conversion_error(const std::string &str)
        : std::runtime_error(
            (vle::fmt("Fail to convert `%1%' in a number") % str).str())
    {}
};

enum class StatusModel { unavailable, sown, raised, flowering, maturity };

inline std::string to_string(StatusModel status)
{
    switch (status) {
    case StatusModel::unavailable:
        return "unavailable";
    case StatusModel::sown:
        return "sown";
    case StatusModel::raised:
        return "raised";
    case StatusModel::flowering:
        return "flowering";
    case StatusModel::maturity:
        return "maturity";
    default:
        throw global_unknown_model_status();
    }
};

inline double stod(const std::string &str)
{
    try {
        return boost::lexical_cast <double>(str);
    } catch (...) {
        throw global_conversion_error(str);
    }
}

inline int stoi(const std::string &str)
{
    try {
        long int ret = boost::lexical_cast <long int>(str);
        return boost::numeric_cast <int>(ret);
    } catch (...) {
        throw global_conversion_error(str);
    }
}

}

#endif
