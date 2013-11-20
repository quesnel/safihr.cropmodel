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
#include "AI.hpp"

namespace safihr {

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

        file.imbue(std::locale::classic());

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
