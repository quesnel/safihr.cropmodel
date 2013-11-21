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

#include <vle/devs/Executive.hpp>
#include <vle/devs/ExecutiveDbg.hpp>
#include <vle/utils/Package.hpp>
#include <vle/utils/Trace.hpp>
#include <algorithm>
#include <vector>
#include "AI.hpp"
#include "Global.hpp"

namespace safihr {

struct data
{
    data(unsigned int id,
         const std::string& name,
         float surface1,
         float surface2,
         vle::devs::Time dmin,
         vle::devs::Time dmax,
         vle::devs::Time duration)
        : id(id), name(name), surface1(surface1), surface2(surface2),
        dmin(dmin), dmax(dmax), duration(duration), result(-1.0)
    {
#ifndef DNDEBUG
        if ((dmax - dmin) != duration)
            throw ai_internal_failure(dmax, dmin, duration);
#endif
    }

    unsigned int id;
    std::string name;
    float surface1;
    float surface2;
    vle::devs::Time dmin;
    vle::devs::Time dmax;
    vle::devs::Time duration;
    vle::devs::Time result;
};

std::ostream& operator<<(std::ostream& out, const data& d)
{
    return out << vle::fmt("%1%;%2%;%3%;%4%;%5%;%6%") % d.id % d.name %
        vle::utils::DateTime::toJulianDayNumber(d.dmin) %
        vle::utils::DateTime::toJulianDayNumber(d.dmax) %
        vle::utils::DateTime::toJulianDayNumber(d.result) %
        (std::abs(d.result - d.dmax));
}

std::ostream& operator<<(std::ostream& out, const std::vector <data> &d)
{
    out << "id_parcelle;libelle_occup;Date-semis;Date-recolte-observee;"
        "Date-recolte-simulee;distance\n";

    std::copy(d.begin(), d.end(),
              std::ostream_iterator <data>(out, "\n"));

    return out;
}

class CompareDateAI : public vle::devs::Executive
{
    std::vector <data> date;
    vle::devs::Time current_time;
    int index;

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
                std::string sur1 = boost::copy_range <std::string>(*i++);
                std::string sur2 = boost::copy_range <std::string>(*i++);
                std::string dmin = boost::copy_range <std::string>(*i++);
                std::string dmax = boost::copy_range <std::string>(*i++);
                std::string dura = boost::copy_range <std::string>(*i++);

                date.emplace_back(date.size(), name, std::stof(sur1),
                                  std::stof(sur2), ai_convert_date(dmin),
                                  ai_convert_date(dmax), std::stof(dura));
            } catch (const std::exception &e) {
                (void)e;
                throw ai_format_failure(i);
            }
            i++;
        }
    }

public:
    CompareDateAI(const vle::devs::ExecutiveInit &init,
                  const vle::devs::InitEventList &evts)
        : vle::devs::Executive(init, evts)
    {
        vle::utils::Package package("safihr.cropmodel");

        initialize_date(package.getDataFile(evts.getString("filename")));

        std::sort(date.begin(), date.end(),
                  [] (const data& lhs,
                      const data& rhs)
                  {
                      return lhs.dmin < rhs.dmin;
                  });

        DTraceModel(vle::fmt("AI need to build: %1% models") % date.size());
    }

    virtual ~CompareDateAI()
    {
    }

    virtual void finish()
    {
        /* sort data vector with data.id member to restore input file format. */
        std::sort(date.begin(), date.end(),
                  [](const data& lhs, const data& rhs)
                  {
                      return lhs.id < rhs.id;
                  });

        std::ofstream result("simulation-outputs.csv");
        if (result.is_open())
            result << std::setprecision(std::numeric_limits <double>::digits10)
                   << std::boolalpha
                   << date
                   << std::endl;
    }

    virtual vle::devs::Time init(const vle::devs::Time &time)
    {
        current_time = time;
        index = 0;

        if (time > date.front().dmin)
            throw ai_internal_failure(time, date.front().dmin);

        int i  = 0;
        std::for_each(date.begin(), date.end(),
                      [this, &i] (const data& /*d*/) {
                          std::string modelname = std::to_string(i++);
                          createModel(modelname,
                                      {"in", "start"},
                                      {"out"},
                                      "dyncrop",
                                      {"species"},
                                      "crop");

                          addConnection("agent", "start", modelname, "start");
                          addConnection(modelname, "out", "agent", "in");
                          addConnection("meteo", "out", modelname, "in");
                      });

        DTraceModel(vle::fmt("CompareDateAI init at %1%") % (date.front().dmin -
                                                             time));

        return date.front().dmin - time;
    }

    virtual vle::devs::Time timeAdvance() const
    {
        if (!date.empty())
            return (*(date.begin() + index)).dmin - current_time;
        else
            return vle::devs::infinity;
    }

    virtual void output(const vle::devs::Time &time,
                        vle::devs::ExternalEventList &output) const
    {
        (void)time;

        DTraceModel(vle::fmt("CompareDateAI: output at %1%") % time);

        if (date.empty())
            return;

        auto low = std::lower_bound(date.begin() + index, date.end(),
                                    (*(date.begin() + index)).dmin,
                                    [] (const data& d, vle::devs::Time value)
                                    {
                                        return d.dmin <= value;
                                    });

        DTraceModel(vle::fmt("CompareDateAI: need to send %1% start message"
                             " (%2%)") % (std::distance(date.begin() + index,
                                                       low)) % index);

        std::for_each(date.begin() + index, low,
                      [&output] (const data& d)
                      {
                          vle::devs::ExternalEvent *ret =
                              new vle::devs::ExternalEvent("start");
                          ret->putAttribute("specie_name",
                                            new vle::value::String(
                                                d.name));
                          ret->putAttribute("landunit_id",
                                            new vle::value::Integer(
                                                d.id));
                          output.push_back(ret);
                      });
    }

    virtual void internalTransition(const vle::devs::Time &time)
    {
        current_time = time;

        if (!date.empty()) {
            auto low = std::lower_bound(date.begin() + index, date.end(),
                                        current_time,
                                        [] (const data& d, vle::devs::Time value)
                                        {
                                            return d.dmin <= value;
                                        });

            index += std::distance(date.begin() + index, low);
        }
    }

    virtual void externalTransition(const vle::devs::ExternalEventList &msgs,
                                    const vle::devs::Time &time)
    {
        current_time = time;

        for (auto &msg : msgs) {
            unsigned int landid =
                std::stoi(msg->attributes().getString("landunit_id"));
            std::string status = msg->attributes().getString("status");

            if (status == "maturity") {
                auto it = std::find_if(date.begin(), date.end(),
                                       [time, landid](const data& d)
                                       {
                                           return d.id == landid;
                                       });

                if (it != date.end() && it->result == -1)
                    it->result = time;
            }
        }
    }

    virtual vle::value::Value * observation(
        const vle::devs::ObservationEvent &event) const
    {
        return vle::devs::Dynamics::observation(event);
    }
};

}

DECLARE_EXECUTIVE_DBG(safihr::CompareDateAI)
