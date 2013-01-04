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

#include <vle/devs/Dynamics.hpp>
#include <vle/devs/DynamicsDbg.hpp>
#include <vle/utils/DateTime.hpp>
#include <vle/utils/Package.hpp>
#include <vle/utils/Trace.hpp>
#include <boost/date_time.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <exception>

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
            (vle::fmt("AI: simulation begin at `%1%', AI data at `%2%'")
             % first % second).str())
    {}
};

struct MinimalistAISpecie
{
    MinimalistAISpecie(const std::string &name,
                       vle::devs::Time dmin,
                       vle::devs::Time dmax)
        : name(name), dmin(dmin), dmax(dmax)
    {}

    std::string name;
    vle::devs::Time dmin;
    vle::devs::Time dmax;
};

std::ostream& operator<<(std::ostream& out, const MinimalistAISpecie& data)
{
    return out << "(" << data.name << ", "
               << boost::numeric_cast <ulong>(data.dmin)
               << " "
               << vle::utils::DateTime::toJulianDay(data.dmin)
               << ", "
               << boost::numeric_cast <ulong>(data.dmax)
               << " "
               << vle::utils::DateTime::toJulianDay(data.dmax)
               << ")";
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector <MinimalistAISpecie> data)
{
    std::copy(data.begin(), data.end(),
              std::ostream_iterator <MinimalistAISpecie>(out, "\n"));

    return out;
}

/**
 * Convert a date into julian day date in vle::devs::Time
 * type. @attention The date must be in format: "dd/mm/yy".
 *
 * @param date A data in the format "dd/mm/yy".
 *
 * @return A julian day date.
 */
static vle::devs::Time ai_convert_date(std::string date)
{
    namespace ba = boost::algorithm;

    ba::split_iterator <std::string::iterator> i, e;
    i = ba::make_split_iterator(date,
                                ba::token_finder(
                                    ba::is_any_of("/"),
                                    ba::token_compress_on));

    int day = std::stoi(boost::copy_range <std::string>(*i++));
    int month = std::stoi(boost::copy_range <std::string>(*i++));
    int year = std::stoi(boost::copy_range <std::string>(*i++));

    boost::gregorian::date d(year, month, day);

    return boost::numeric_cast <vle::devs::Time>(d.julian_day());
}

class MinimalistAI : public vle::devs::Dynamics
{
    std::vector <MinimalistAISpecie> date;
    std::map <std::string, long int> lev;
    vle::devs::Time current_time;
    uint landunit_id;

    void initialize_date(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw ai_open_failure(filename);

        std::string line;
        std::getline(file, line); /* read the header and forget it*/

        uint i = 0;
        while (file) {
            if (!std::getline(file, line))
                break;

            try {
                boost::algorithm::split_iterator <std::string::iterator> i, e;
                i = make_split_iterator(
                    line,
                    token_finder(
                        boost::algorithm::is_any_of(";"),
                        boost::algorithm::token_compress_on));

                std::string name = boost::copy_range <std::string>(*i++);
                std::string begin = boost::copy_range <std::string>(*i++);
                std::string end = boost::copy_range <std::string>(*i++);

                vle::devs::Time dmin = ai_convert_date(begin);
                vle::devs::Time dmax = ai_convert_date(end);

                date.emplace_back(name, dmin, dmax);
            } catch (const std::exception &e) {
                (void)e;
                throw ai_format_failure(i);
            }
            i++;
        }
    }

public:
    MinimalistAI(const vle::devs::DynamicsInit &init,
                 const vle::devs::InitEventList &evts)
        : vle::devs::Dynamics(init, evts), current_time(vle::devs::infinity),
          landunit_id(1)
    {
        vle::utils::Package package("safihr.cropmodel");

        initialize_date(package.getDataFile(evts.getString("filename")));

        std::sort(date.begin(), date.end(),
                  [] (const MinimalistAISpecie& lhs,
                      const MinimalistAISpecie& rhs)
                  {
                      return lhs.dmin < rhs.dmin;
                  });

        DTraceModel(vle::fmt("AI Scheduller: %1%") % date);
    }

    virtual ~MinimalistAI()
    {}

    virtual vle::devs::Time init(const vle::devs::Time &time)
    {
        current_time = time;

        if (time > date.front().dmin)
            throw ai_internal_failure(time, date.front().dmin);

        return date.front().dmin - time;
    }

    virtual vle::devs::Time timeAdvance() const
    {
        if (!date.empty())
            return date.front().dmin - current_time;
        else
            return vle::devs::infinity;
    }

    virtual void output(const vle::devs::Time &time,
                        vle::devs::ExternalEventList &output) const
    {
        (void)time;

        if (date.empty())
            return;

        auto low = std::lower_bound(date.begin(), date.end(),
                                    date.front().dmin,
                                    [] (const MinimalistAISpecie& specie,
                                        vle::devs::Time value)
                                    {
                                        return specie.dmin <= value;
                                    });

        uint landunit = landunit_id;
        std::for_each(date.begin(), low,
                      [&output, &landunit] (const MinimalistAISpecie& specie)
                      {
                          vle::devs::ExternalEvent *ret =
                              new vle::devs::ExternalEvent("start");
                          ret->putAttribute("specie_name",
                                            new vle::value::String(
                                                specie.name));
                          ret->putAttribute("landunit_id",
                                            new vle::value::Integer(
                                                landunit++));
                          output.push_back(ret);
                      });
    }

    virtual void internalTransition(const vle::devs::Time &time)
    {
        current_time = time;

        if (!date.empty()) {
            auto low = std::lower_bound(date.begin(), date.end(),
                                        current_time,
                                        [] (const MinimalistAISpecie& specie,
                                            vle::devs::Time value)
                                        {
                                            return specie.dmin <= value;
                                        });

            landunit_id += std::distance(date.begin(), low);

            date.erase(date.begin(), low);
        }
    }

    virtual void externalTransition(const vle::devs::ExternalEventList &msgs,
                                    const vle::devs::Time &time)
    {
        current_time = time;

        for (auto &msg : msgs) {
            if (msg->attributes().exist("lev"))
                lev[msg->attributes().getString("specie")] = 1;
            else if (msg->attributes().exist("harvest"))
                lev[msg->attributes().getString("specie")] = 2;
            else
                lev[msg->attributes().getString("specie")] = 0;
        }
    }

    virtual vle::value::Value * observation(
        const vle::devs::ObservationEvent &event) const
    {
        auto found = lev.find(event.getPortName());

        if (found != lev.end())
            return new vle::value::Integer(found->second);

        return vle::devs::Dynamics::observation(event);
    }
};

}

DECLARE_DYNAMICS_DBG(safihr::MinimalistAI)
