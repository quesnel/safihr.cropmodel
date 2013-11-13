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
#include <vle/utils/Package.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <string>
#include <exception>

namespace safihr {

struct file_generator_open : std::runtime_error
{
    explicit file_generator_open(const std::string &filepath)
        : std::runtime_error(
            (vle::fmt("can not open file `%1%'") % filepath).str())
    {}
};

struct file_generator_format : std::runtime_error
{
    explicit file_generator_format(int line)
        : std::runtime_error(
            (vle::fmt("fail to read line `%1%'") % line).str())
    {}
};

class FileReader
{
public:
    std::ifstream m_input;
    std::string m_date;
    double m_tn;
    double m_tmin;
    double m_tmax;
    double m_tmoy;
    ulong m_line;

    FileReader()
        : m_tn(1.0), m_tmin(0.0), m_tmax(0.0), m_tmoy(0.0), m_line(1)
    {}

    ~FileReader()
    {}

    void open(const std::string &filepath)
    {
        if (m_input.is_open())
            close();

        m_input.open(filepath.c_str());

        if (!m_input)
            throw file_generator_open(filepath);

        m_input.imbue(std::locale::classic());

        read_header();
    }

    void close()
    {
        m_input.close();

        m_tn = 1.0;
        m_tmin = 0.0;
        m_tmax = 0.0;
        m_tmoy = 0.0;
        m_line = 1;
    }

    bool next()
    {
        std::string line;

        if (not std::getline(m_input, line))
            return false;

        ++m_line;

        boost::algorithm::split_iterator <std::string::iterator> i, e;
        i = make_split_iterator(line, token_finder(
                                    boost::algorithm::is_any_of(";"),
                                    boost::algorithm::token_compress_on));

        try {
            m_date = boost::copy_range <std::string>(*i++);
            m_tmin = std::stod(boost::copy_range <std::string>(*i++));
            m_tmax = std::stod(boost::copy_range <std::string>(*i++));
            m_tmoy = std::stod(boost::copy_range <std::string>(*i++));
        } catch (const std::exception &e) {
            (void)e;
            throw file_generator_format(m_line);
        }

        return true;
    }

private:
    void read_header()
    {
        std::string line;
        if (not std::getline(m_input, line))
            throw file_generator_format(0);
    }
};

class Meteo : public vle::devs::Dynamics
{
    FileReader m_gen;
    bool m_is_started;

public:
    Meteo(const vle::devs::DynamicsInit &init,
                  const vle::devs::InitEventList &evts)
        : vle::devs::Dynamics(init, evts)
    {
        vle::utils::Package package("safihr.cropmodel");

        m_gen.open(package.getDataFile(evts.getString("filename")));
    }

    virtual ~Meteo()
    {}

    virtual vle::devs::Time init(const vle::devs::Time &time)
    {
        (void)time;

        m_is_started = false;

        return 0.0;
    }

    virtual vle::devs::Time timeAdvance() const
    {
        (void)time;

        return m_gen.m_tn;
    }

    virtual void internalTransition(const vle::devs::Time &time)
    {
        (void)time;

        m_is_started = true;

        if (!m_gen.next())
            throw file_generator_format(m_gen.m_line);
    }

    virtual void output(const vle::devs::Time &time,
                        vle::devs::ExternalEventList &output) const
    {
        (void)time;

        if (m_is_started) {
            vle::devs::ExternalEvent *ret = new vle::devs::ExternalEvent("out");
            vle::value::Map &msg = ret->attributes();

            msg.addDouble("tmin", m_gen.m_tmin);
            msg.addDouble("tmax", m_gen.m_tmax);
            msg.addDouble("tmoy", m_gen.m_tmoy);

            output.push_back(ret);
        }
    }

    virtual vle::value::Value * observation(
        const vle::devs::ObservationEvent &event) const
    {
        if (event.onPort("tmin"))
            return new vle::value::Double(m_gen.m_tmin);

        if (event.onPort("tmax"))
            return new vle::value::Double(m_gen.m_tmax);

        if (event.onPort("tmoy"))
            return new vle::value::Double(m_gen.m_tmoy);

        return vle::devs::Dynamics::observation(event);
    }
};

}

DECLARE_DYNAMICS_DBG(safihr::Meteo)
