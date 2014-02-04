/*
 * Copyright (C) 2013-2014 INRA
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
#include <boost/algorithm/string.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <memory>
#include <algorithm>
#include <valarray>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <exception>
#include "Global.hpp"

namespace safihr {

struct crop_model_internal_failure : std::runtime_error
{
    explicit crop_model_internal_failure()
        : std::runtime_error("Crop model: internal error, contact developer")
    {}
};

struct crop_model_failure : std::runtime_error
{
    explicit crop_model_failure(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

struct crop_model_file_open_failure : std::runtime_error
{
    explicit crop_model_file_open_failure(const std::string &filepath)
        : std::runtime_error(
            (vle::fmt("Crop model: fail to open data `%1%' file")
             % filepath).str())
    {}
};

struct crop_model_file_failure : std::runtime_error
{
    explicit crop_model_file_failure(uint line)
        : std::runtime_error(
            (vle::fmt("Crop model: fail to read data at line %1%")
             % line).str())
    {}
};

struct crop_model_unknown_specie : std::runtime_error
{
    explicit crop_model_unknown_specie(const std::string &speciename)
        : std::runtime_error(
            (vle::fmt("Crop model: unknown specie `%1%'") % speciename).str())
    {}
};

constexpr double infinity = std::numeric_limits <double>::infinity();

struct Specie
{
    std::string name;
    std::valarray <double> data;

    enum DataId {
        LEV_AMF = 0,
        AMF_LAX,
        SEM_LEV,
        LEV_MAT,
        TBASE,
        TMAXDEV,
        PBASE,
        POPT,
        TFROID,
        AMPFROID,
        VBASE,
        VSAT,
        ADENS,
        CROIRAC,
        BDENS,
        LAICOMP,
        TCOUVMAX,
        PENTECOUVMAX,
        INFRECOUV,
        HMAX,
        HBASE,
        GRAINES_REC
    };

    Specie(const std::string &name)
        : name(name), data(0.0, 22)
    {}

    Specie()
        : name(), data(0.0, 22)
    {}
};

std::ostream& operator<<(std::ostream& out, const Specie& specie)
{
    std::streamsize sz = out.precision(); /* keep the old precision
                                           * value to restore it at
                                           * the end of the stream.
                                           */

    return out << std::setprecision(std::numeric_limits <double>::digits10)
               << "NAME " << specie.name
               << " LEV_AMF " << specie.data[Specie::LEV_AMF]
               << " AMF_LAX " << specie.data[Specie::AMF_LAX]
               << " SEM_LEV " << specie.data[Specie::SEM_LEV]
               << " LEV_MAT " << specie.data[Specie::LEV_MAT]
               << " TBASE " << specie.data[Specie::TBASE]
               << " TMAXDEV " << specie.data[Specie::TMAXDEV]
               << " PBASE " << specie.data[Specie::PBASE]
               << " POPT " << specie.data[Specie::POPT]
               << " TFROID " << specie.data[Specie::TFROID]
               << " AMPFROID " << specie.data[Specie::AMPFROID]
               << " VBASE " << specie.data[Specie::VBASE]
               << " VSAT " << specie.data[Specie::VSAT]
               << " ADENS " << specie.data[Specie::ADENS]
               << " CROIRAC " << specie.data[Specie::CROIRAC]
               << " BDENS " << specie.data[Specie::BDENS]
               << " LAICOMP " << specie.data[Specie::LAICOMP]
               << " TCOUVMAX " << specie.data[Specie::TCOUVMAX]
               << " PENTECOUVMAX " << specie.data[Specie::PENTECOUVMAX]
               << " INFRECOUV " << specie.data[Specie::INFRECOUV]
               << " HMAX " << specie.data[Specie::HMAX]
               << " HBASE " << specie.data[Specie::HBASE]
               << "GRAINES_REC " << specie.data[Specie::GRAINES_REC]
               << "\n"
               << std::setprecision(sz);
        ;
}

struct SpecieFileReader
{
    std::ifstream file;

    SpecieFileReader(const std::string &filename)
        : file(filename)
    {
        if (!file.is_open())
            throw crop_model_file_open_failure(filename);

        file.imbue(std::locale::classic());
    }

    Specie get(const std::string &speciename)
    {
        uint line_id = 1;
        std::string line;

        std::getline(file, line); /* Forget the header of the
                                   * file. */

        if (file.eof() || file.fail())
            throw crop_model_file_failure(line_id);

        do {
            line_id++;
            std::getline(file, line);

            if (file.fail())
                break;

            try {
                std::vector <std::string> result;
                boost::algorithm::split(result, line,
                                        boost::algorithm::is_any_of(";"),
                                        boost::algorithm::token_compress_on);

                if (result.at(0) == speciename) {
                    Specie specie(speciename);

                    std::transform(result.begin() + 1,
                                   result.end(),
                                   std::begin(specie.data),
                                   [] (const std::string &str) -> double
                                   {
                                       if (str == "infinity")
                                           return infinity;
                                       else
                                           return safihr::stod(str);
                                   });

                    DTraceModel((vle::fmt("%1%") % specie).str());

                    //assert(specie.data[Specie::SEM_LEV] == 150);
                    //assert(specie.data[Specie::LEV_MAT] == 1232);
                    //assert(specie.data[Specie::TBASE] == 0);
                    //assert(specie.data[Specie::TMAXDEV] == 28);
                    //assert(specie.data[Specie::TFROID] == 6.5);
                    //assert(specie.data[Specie::PBASE] == 6.3);

                    return specie;
                }
            } catch (const std::exception &e) {
                (void)e;
                throw crop_model_file_failure(line_id);
            }
        } while (!file.eof());

        throw crop_model_unknown_specie(speciename);
    }
};

struct Model
{
    StatusModel status;
    vle::devs::Time day_lev;

    Model(StatusModel status)
        : status(status), day_lev(vle::devs::infinity)
    {}

    virtual ~Model()
    {}

    virtual std::string name() const = 0;

    virtual StatusModel compute(vle::devs::Time time, double tmoy) = 0;
};

struct LinModel : Model
{
    double sum;

    LinModel()
        : Model(StatusModel::sown), sum(0.0)
    {}

    virtual ~LinModel()
    {}

    virtual std::string name() const
    {
        return "LIN";
    }

    virtual StatusModel compute(vle::devs::Time time, double tmoy) override
    {
        (void)time;

        sum += tmoy - 5;

        switch (status) {
        case StatusModel::unavailable:
        case StatusModel::maturity:
            break;
        case StatusModel::sown:
            if (sum >= 50.0) {
                status = StatusModel::raised;
                Model::day_lev = time;
                sum = 0.0;
            }
            break;
        case StatusModel::raised:
            if (sum >= 500.0) {
                status = StatusModel::flowering;
                sum = 0.0;
            }
            break;
        case StatusModel::flowering:
            if (sum >= 400) {
                status = StatusModel::maturity;
                sum = 0.0;
            }
            break;
        }

        return status;
    }
};

struct GenericModel : Model
{
    const Specie specie;
    double latitude;
    double tdev_sum;
    std::valarray <double> fp;
    std::valarray <double> fp_leapyear;
    double udev;
    double vdd;

    void initialize(std::valarray <double> &fp, vle::devs::Time nbday)
    {
        fp.resize(boost::numeric_cast <size_t>(nbday));

        if (specie.data[Specie::PBASE] != infinity) {
            double lat = M_PI * latitude / 180.0;
            double jjulian = 1;

            for (uint i = 0, end = fp.size(); i < end; ++i) {
                double dec = std::asin(
                    0.3978 * std::sin(
                        (2.0 * M_PI * (jjulian - 80.0) / nbday) +
                        ((0.0335 * (std::sin(2.0 * M_PI * jjulian) -
                                    std::sin(2.0 * M_PI * 80.0))) / nbday)));

                double ph = 24.0 *
                    (std::acos(
                        ((-0.10453 / std::cos(lat)) *
                         std::cos(dec)) - (std::tan(lat) *
                                           std::tan(dec)))) / M_PI;

                fp[i] = std::max(0.0,
                                 (ph - specie.data[Specie::PBASE]) /
                                 (specie.data[Specie::POPT] -
                                  specie.data[Specie::PBASE]));
                ++jjulian;
            }
        } else {
            fp = 1;
        }
    }

    GenericModel(vle::devs::Time time, const Specie &specie, double latitude)
        : Model(StatusModel::sown), specie(specie), latitude(latitude),
        tdev_sum(0.0), udev(0.0), vdd(0.0)
    {
        (void)time;

        initialize(fp, 365.0);
        initialize(fp_leapyear, 366.0);

        udev = 0.0;
        vdd = 0.0;
        tdev_sum = 0.0;
    }

    virtual std::string name() const
    {
        return specie.name;
    }

    virtual StatusModel compute(vle::devs::Time time, double tmoy) override
    {
        if (vle::utils::DateTime::dayOfYear(time) == 1) {
            if (vle::utils::DateTime::isLeapYear(time))
                std::swap(fp, fp_leapyear);
            else if (vle::utils::DateTime::isLeapYear(time - 1.0))
                std::swap(fp, fp_leapyear);
        }

        double tdev = (tmoy >= specie.data[Specie::TMAXDEV]) ?
            //specie.data[Specie::TMAXDEV] :
            std::max(0.0, specie.data[Specie::TMAXDEV]
                     - specie.data[Specie::TBASE]) :
            std::max(0.0, tmoy - specie.data[Specie::TBASE]);

        tdev_sum += tdev;

        if (tdev_sum >= specie.data[Specie::SEM_LEV] && Model::day_lev ==
            vle::devs::infinity) {
            Model::day_lev = time;
            status = StatusModel::raised;

            DTraceModel((vle::fmt("[%1%] is lev\n") % specie.name).str());
        }

        double jvi = (specie.data[Specie::TFROID] == infinity) ? 0.0 :
            std::max(0.0, (1.0 - (((specie.data[Specie::TFROID] - tmoy) /
                                   specie.data[Specie::AMPFROID])
                                  * ((specie.data[Specie::TFROID] - tmoy) /
                                     specie.data[Specie::AMPFROID]))));

        double old_vdd = vdd;
        vdd = time < Model::day_lev ? 0.0 : old_vdd + jvi;

        if (status == StatusModel::raised) {
            double fv = specie.data[Specie::VBASE] == 1.0 ? 1.0 :
                std::max(0.0,
                         std::min(1.0,
                                  ((old_vdd - specie.data[Specie::VBASE]) /
                                   (specie.data[Specie::VSAT]
                                    - specie.data[Specie::VBASE]))));

            udev += tdev * fv * fp[vle::utils::DateTime::dayOfYear(time)];

            DTraceModel(vle::fmt("%1%: vdd=%2% jvi=%3% udev=%4%"
                                 " fv=%5% fp=%6% (day=%7%\n") % specie.name %
                        vdd % jvi % udev % fv %
                        fp[vle::utils::DateTime::dayOfYear(time)] %
                        vle::utils::DateTime::dayOfYear(time));

            if (udev > specie.data[Specie::LEV_MAT]) {
                status = StatusModel::maturity;
                DTraceModel((vle::fmt("[%1%] harvestable %2% > %3%") %
                             specie.name % udev %
                             specie.data[Specie::LEV_MAT]).str());
            }
        }

        return status;
    }
};

class GenericCropModel : public vle::devs::Dynamics
{
    std::shared_ptr <Model> m_model;
    double m_latitude;
    double m_tmoy;
    vle::devs::Time m_last_date;
    vle::devs::Time m_next_date;
    vle::devs::Time m_sigma;
    StatusModel previous_status, new_status;
    bool is_sown;
    std::string m_paramfilename;

public:
    GenericCropModel(const vle::devs::DynamicsInit &dinit,
                     const vle::devs::InitEventList &evts)
        : vle::devs::Dynamics(dinit, evts), m_latitude(172.0), m_tmoy(0.0),
        m_last_date(vle::devs::negativeInfinity),
        m_next_date(vle::devs::infinity),
        m_sigma(vle::devs::infinity)
    {
        m_latitude = evts.getDouble("latitude");
        m_paramfilename = evts.getString("filename");
    }

    virtual ~GenericCropModel()
    {}

    virtual vle::devs::Time init(const vle::devs::Time &time)
    {
        (void)time;

        DTraceModel(vle::fmt("GenericCropModel %1% builded\n") %
                    getModelName());

        previous_status = StatusModel::unavailable;
        new_status = StatusModel::unavailable;
        is_sown = false;

        return vle::devs::infinity;
    }


    virtual vle::devs::Time timeAdvance() const
    {
        return is_sown ? m_sigma : vle::devs::infinity;
    }

    virtual void output(const vle::devs::Time &time,
                        vle::devs::ExternalEventList &output) const
    {
        (void)time;

        if (is_sown && previous_status != new_status) {
            vle::devs::ExternalEvent *ret = new vle::devs::ExternalEvent("out");
            ret->putAttribute("landunit_id",
                              new vle::value::String(getModelName()));
            ret->putAttribute("specie",
                              new vle::value::String(m_model->name()));
            ret->putAttribute("day_lev",
                              new vle::value::Double(m_model->day_lev));
            ret->putAttribute("status",
                              new vle::value::String(to_string(new_status)));

            output.push_back(ret);
        }
    }

    virtual void internalTransition(const vle::devs::Time &time)
    {
        if (is_sown) {
            previous_status = new_status;
            new_status = m_model->compute(time, m_tmoy);

            m_last_date = time;
            m_sigma = 1.0;
            m_next_date = time + m_sigma;
        }
    }

    virtual void externalTransition(const vle::devs::ExternalEventList &msgs,
                                    const vle::devs::Time &time)
    {
        if (!is_sown) {
            auto it = msgs.begin();
            do {
                it = std::find_if(it, msgs.end(),
                                  [](const vle::devs::ExternalEvent *event)
                                  {
                                      return event->onPort("start");
                                  });

                if (it != msgs.end()) {
                    std::string specie_name =
                        (*it)->attributes().getString("specie_name");
                    uint landunit_id =
                        (*it)->attributes().getInt("landunit_id");

                    if (getModelName() == std::to_string(landunit_id)) {
                        if (specie_name == "LIN") {
                            m_model = std::shared_ptr <Model>(new LinModel());
                        } else {
                            vle::utils::Package package("safihr.cropmodel");
                            SpecieFileReader filereader(
                                package.getDataFile(m_paramfilename));
                            m_model = std::shared_ptr <Model>(
                                new GenericModel(time,
                                                 filereader.get(specie_name),
                                                 m_latitude));
                        }

                        m_last_date = time;
                        m_next_date = time + 1.0;
                        m_sigma = m_next_date - m_last_date;
                        is_sown = true;

                        DTraceModel((vle::fmt("Parcelle %1% is started with"
                                              " %2%\n")
                                     % landunit_id % specie_name).str());
                    }
                    ++it;
                }
            } while (it != msgs.end());
        }

        if (is_sown) {
            if (m_last_date >= time && time < m_next_date) {
                m_last_date = time;
                m_sigma = m_next_date - m_last_date;
            } else if (time >= m_next_date) {
                m_last_date = time;
                m_sigma = 0.0;
            } else {
                assert(false);
            }

            auto found = std::find_if(msgs.begin(), msgs.end(),
                                      [] (const vle::devs::ExternalEvent *event)
                                      {
                                          return event->onPort("in");
                                      });

            if (found != msgs.end()) {
                m_tmoy = (*found)->attributes().getDouble("tmoy");
            }
        }
    }

    virtual vle::value::Value * observation(
        const vle::devs::ObservationEvent &event) const
    {
        if (is_sown) {
            if (event.onPort("name"))
                return new vle::value::String(m_model->name());

            if (event.onPort("status"))
                return new vle::value::String(to_string(new_status));
        }

        const GenericModel* mdl = dynamic_cast <GenericModel*>(m_model.get());
        if (mdl) {
            if (event.onPort("udev"))
                return new vle::value::Double(mdl->udev);

            if (event.onPort("tdev"))
                return new vle::value::Double(mdl->tdev_sum);
        }

        return vle::devs::Dynamics::observation(event);
    };
};

}

DECLARE_DYNAMICS_DBG(safihr::GenericCropModel)
