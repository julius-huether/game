#include <stdio.h>
#include <iostream>
#include <vector>
#include <sstream>

#include <stdint.h>
#include <boost/regex.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "executor.hpp"
#include "strutil.hpp"

using namespace ourapi;
using std::vector;
using boost::property_tree::ptree;
using std::make_pair;
using boost::lexical_cast;
using boost::bad_lexical_cast;
using boost::format;
using boost::regex_search;
using boost::match_default;
using boost::match_results;
using boost::regex;


Executor::Executor()
{
}

bool Executor::diskinfo(const set<string>& args, outputType type, 
        string& response)
{
    const char *command = "df | sed 's/ \\+/ /g'  | tail -n +2 ";
    char line[255];
    vector<string> tokens;
    int i = 0,j;
    bool spaceinfo = false;
    bool totalparts = false;
    uint64_t totalspace = 0;
    uint64_t usedspace = 0;
    int32_t partnum = 0;

    FILE *fp = popen(command, "r");
    if (!fp){
        return false;
    }
    while (fgets(line, 255, fp) != 0){
        response += string(line);
    }
    fclose(fp);

    if (args.find("spaceinfo") != args.end()) {
        spaceinfo = true;
    }
    if (args.find("totalparts") != args.end()) {
        totalparts = true;
    }


    StrUtil::splitString( response, " \t\n", tokens); 
    
    j = tokens.size();
    ptree diskinforoot ;
    ptree diskinfo;

    ptree::iterator  ptit = diskinforoot.push_back(make_pair("diskinfo", diskinfo ));
    ptree::iterator pit ;
    while (i < j) {
        {
            ptree temp;
            pit = ptit->second.push_back(make_pair("FileSystem", temp));
        }
        pit->second.push_back(make_pair("Name", tokens[i++]));
        try {
            if (spaceinfo) {
                totalspace += lexical_cast<uint64_t>(tokens[i]);
            }
            pit->second.push_back(make_pair("Size", tokens[i++]));
            usedspace += lexical_cast<uint64_t>(tokens[i]);
            pit->second.push_back(make_pair("Used", tokens[i++]));

        } catch ( bad_lexical_cast& e) {
        }
        pit->second.push_back(make_pair("Avail", tokens[i++]));
        pit->second.push_back(make_pair("PercentUse", tokens[i++]));
        pit->second.push_back(make_pair("MountedOn", tokens[i++]));
        partnum++;
    }

    if (spaceinfo) {
        ptree temp;
        format fmter("%1%");
        pit = ptit->second.push_back(make_pair("SpaceInfo", temp));
        fmter % totalspace;
        pit->second.push_back(make_pair("TotalSpace", fmter.str()));
        fmter.clear();
        fmter % usedspace;
        pit->second.push_back(make_pair("UsedSpae", fmter.str()));
        fmter.clear();

    }

    if (totalparts) {
        ptree temp;
        format fmter("%1%");
        fmter % partnum;
        ptit->second.push_back(make_pair("TotalParts", fmter.str()));
        fmter.clear();
    }

    _generateOutput(&diskinforoot, type, response);
    std::cout << response << std::endl;
    return true;
}

bool Executor::procinfo(const set<string>& args, outputType type, 
        string& response)
{
    const char *command = "ps auxef | tail -n +2 |awk ' { printf \"%s %s %s %s \", $1, $2, $3, $3 ; for (i = 11; i <= NF; i++) {printf \"%s \", $i }  print \"\" }  ' ";
    char line[8096];
    FILE *fp = popen(command, "r");

    if (!fp) {
        return false;
    }

    string read_line;
    ptree prcinforoot ;
    ptree prcinfo;
    string::const_iterator start, end;
    match_results<string::const_iterator > what;
    ptree::iterator  ptit = prcinforoot.push_back(make_pair("prcinfo", prcinfo ));
    ptree::iterator pit;
    regex expression("(.*?) (.*?) (.*?) (.*?) (.*)");  
    ptree temp;
    bool percentcpu = false;
    bool percentmemory = false; 

    if (args.find("percentcpu") != args.end()) {
        percentcpu = true;
    }
    if (args.find("percentmemory") != args.end()) {
        percentmemory = true;
    }

    while (fgets(line, 8096, fp) != 0){
        read_line = line;
        start = read_line.begin();
        end = read_line.end();
        if (!regex_search(start, end, what, expression, match_default)){
            continue;
        }
        if (what.size() != 6){
            continue;
        }   
        pit = ptit->second.push_back(make_pair("process", temp));
        pit->second.push_back(make_pair("owner", string(what[1].first, what[1].second)));
        pit->second.push_back(make_pair("processid", string(what[2].first, what[2].second)));
        if (percentcpu)
            pit->second.push_back(make_pair("percentcpu", string(what[3].first, what[3].second)));
        if (percentmemory)
            pit->second.push_back(make_pair("percentmemory", string(what[4].first, what[4].second)));
        pit->second.push_back(make_pair("processcommand", string(what[5].first, what[5].second)));
    }
    fclose(fp);    
    _generateOutput(&prcinforoot, type, response);
    std::cout << response << std::endl;
    return true;
}

bool Executor::sysinfo(const set<string>& args, outputType type, 
        string& response)
{
    const char *commandcpu = "cat /proc/cpuinfo |  sed 's/\\s\\+: /:/g'";
    const char *commandmemory = "cat /proc/meminfo |  sed 's/:\\s\\+/:/g'";
    const char *commandos = "uname -a";
    FILE *fp;
    char commandout[1048];
    string line;
    ptree sysinforoot ;
    ptree sysinfo;
    ptree::iterator  ptit = sysinforoot.push_back(make_pair("sysinfo", sysinfo ));

    while (args.empty() || args.find("cpus") != args.end()) {
        fp = popen(commandcpu, "r");
        if (!fp)
            break;
        ptree temp;
        string field;
        string value;
        size_t index;
        ptree::iterator pit;
        while (fgets(commandout, 1048, fp) != 0){
            line = commandout;
            StrUtil::eraseAllChars(line, ")( \r\n\t");
            if (strncasecmp(line.c_str(),"processor:", 10) == 0) {
                pit = ptit->second.push_back(make_pair("cpus", temp));
            }
            index = line.find(":");
            if (string::npos == index)
                continue;
            field = line.substr(0, index);
            value = line.substr(index + 1);
            pit->second.push_back(make_pair(field, value));
        }
        fclose(fp);
        break;
    }
    
    while (args.empty()  ||  args.find("memory") != args.end()) {
        fp = popen(commandmemory, "r");
        if (!fp)
            break;
        ptree temp;
        string field;
        string value;
        size_t index;
        ptree::iterator pit = ptit->second.push_back(make_pair("memory", temp));
        while (fgets(commandout, 1048, fp) != 0){
            line = commandout;
            StrUtil::eraseAllChars(line, ")( \n\r\t");
            index = line.find(":");
            if (string::npos == index)
                continue;
            field = line.substr(0, index );
            value = line.substr(index + 1);
            pit->second.push_back(make_pair(field, value));
        }
        fclose(fp);
        break;
    }
    while (args.empty() || args.find("os") != args.end()) {
        fp = popen(commandos, "r");
        if (!fp)
            break;
        if (fgets(commandout, 1048, fp) == 0) {
            fclose(fp);
            break;
        }
        line = commandout;
        ptree temp;
        string field;
        string value;
        size_t index;
        ptree::iterator pit = ptit->second.push_back(make_pair("os", temp));
        pit->second.push_back(make_pair("osdetails", line));
        fclose(fp);
        break;
    }

    _generateOutput(&sysinforoot, type, response);
    std::cout << response << std::endl;

    return true;
}

void Executor::_generateOutput(void *data, outputType type, string& output)
{
    std::ostringstream ostr;
    ptree *pt = (ptree *) data;
    if (TYPE_JSON == type)
        write_json(ostr, *pt);
    else if (TYPE_XML == type)
        write_xml(ostr, *pt);

    output = ostr.str();
}

