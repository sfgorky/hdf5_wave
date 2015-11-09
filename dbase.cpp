#include <stdio.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <vector>


#include "H5Cpp.h"
#ifndef H5_NO_NAMESPACE
using namespace H5;
#endif

const bool bDebugThis = true;

class Waveform
{
    public:
        Waveform( )
        { }
    
        void resize(const size_t size)
        { m_data.resize(2*size);  }
    
    
        size_t size( )const
        { return m_data.size()/2; }
    
        const std::string& name( )const
        { return m_name; }
    
        const std::string& x_unit( )const
        { return m_xunit; };
    
        const std::string& y_unit( )const
        { return m_yunit; };
    
        const double* data( )const
        { return m_data.data(); }
    
    
        void addPoint(const double x, const double y)
        { m_data.push_back(x); m_data.push_back(y); }
    
    
        std::vector<double> m_data; // (x, y, x, y,...)
        std::string m_name;
        std::string m_xunit;
        std::string m_yunit;
    
    private:
    
};

////////////////////////////////////////////////////////////////////////////////

bool readWaveform(Waveform& wave, const std::string& fname)
{
    bool bOk = true;
    FILE* fin = fopen(fname.c_str(), "rt");
    if(fin)
    {
        int lineidx = 0;
        char sBuffer[1024];
        while(!feof(fin))
        {
            if(fgets(sBuffer, sizeof(sBuffer), fin))
            {
                if(sBuffer[0] == 0 || sBuffer[0] == '\n') continue;
                
                if((lineidx == 0) && sBuffer[0] == '#')
                {
                    //#SIG <sigName> <nbPts> xunit yunit
                    char s1[16];
                    char s2[1024];
                    char s3[16];
                    char s4[16];
                    int nbPts=0;
                    sscanf(sBuffer, "%s %s %d %s %s", s1, s2, &nbPts, s3, s4);
                    wave.m_xunit = s3;
                    wave.m_yunit = s4;
                    wave.m_name = s2;
                }
                else if(sBuffer[0] != '#' && (sBuffer[0] != ';'))
                {
                    double x = 0;
                    double y = 0;
                    if(sscanf(sBuffer, "%lg %lg", &x, &y) == 2)
                    {
                        wave.addPoint(x, y);
                    }
                }
                lineidx++;
            }
        }
        bOk = true;
        fclose(fin);
    }
    return bOk;
}

////////////////////////////////////////////////////////////////////////////////
// Creates an 'empty hdf5 file 'fname'
// If file exists, the previous file will be erased
bool createNewFile(const std::string& fname)
{
    bool bOk = true;
    if(bDebugThis) fprintf(stdout, "createNewFile(%s) \n", fname.c_str());

    try
    {
        Exception::dontPrint();
        H5File file(fname, H5F_ACC_TRUNC);
        bOk = true;
    }
    
    // catch failure caused by the H5File operations
    catch(FileIException error)
    {
        error.printError();
        bOk = false;
    }
    // catch failure caused by the DataSet operations
    catch(DataSetIException error)
    {
        error.printError();
        bOk = false;;
    }
    // catch failure caused by the DataSpace operations
    catch(DataSpaceIException error)
    {
        error.printError();
        bOk = false;
    }
    return bOk;
}

////////////////////////////////////////////////////////////////////////////////
// returns a group 'groupName' on a file. If the group is does not exists, it is
// created
H5::Group getGroup(H5::H5File& file, const std::string& groupName)
{
    try
    {
        if(bDebugThis) fprintf(stdout, "getGroup(%s) openGroup\n", groupName.c_str());
        H5::Exception::dontPrint();
        return file.openGroup(groupName);
    }
    catch(...)
    {
        if(bDebugThis) fprintf(stdout, "getGroup(%s) createGroup\n", groupName.c_str());
        try
        {
            return file.createGroup(groupName);
        }
        catch(H5::Exception error)
        {
            error.printError();
            if(bDebugThis) fprintf(stdout, "getGroup(%s) Error\n", groupName.c_str());
            return Group();
        }
    }
    if(bDebugThis) fprintf(stdout, "getGroup(%s) Error\n", groupName.c_str());
    return H5::Group();
}


////////////////////////////////////////////////////////////////////////////////

void addStringAttribute(DataSet&           dataset,
                        const std::string& name,
                        const std::string& value)
{
    if(name.empty()) return;
    
    const int nbAttribute = 1;
    const hsize_t dims[1] = { nbAttribute };
    
    // Create the data space for the attribute.
    DataSpace attr_dataspace = DataSpace (1, dims);
    
    // Create new string datatype for attribute
    H5::StrType strdatatype(H5::PredType::C_S1, value.size()+1); // of length 256 characters

    // Set up write buffer for attribute
    H5std_string attr_data = value.c_str();

    // Create attribute and write to it
    Attribute myatt_in = dataset.createAttribute(name.c_str(), strdatatype, attr_dataspace);
    myatt_in.write(strdatatype, attr_data);
}

////////////////////////////////////////////////////////////////////////////////
// adds a waveform to 'new dataset' under the groupname
bool createDataSet(const std::string& fname,
                   const std::string& _groupName,
                   const Waveform&    waveform)
{
    bool bOk = true;
    if(bDebugThis) fprintf(stdout, "createDataSet(fname:'%s', group:'%s', wave:'%s')\n",
                           fname.c_str(), _groupName.c_str(), waveform.name().c_str());
    try
    {
        //Exception::dontPrint();
        
        // tries to open the file - It assuems that the file exists already.

        //H5File file(fname, H5F_ACC_TRUNC);
        H5::H5File file(fname, H5F_ACC_RDWR);
        
        
        hsize_t dim[2];
        dim[0] = waveform.size();
        dim[1] = 2;

        // the length of dim
        int rank = sizeof(dim) / sizeof(hsize_t);
        
        DataSpace dataspace(rank, dim);
       
        const std::string groupName = std::string("/") + _groupName;
 
        Group group = getGroup(file, groupName);
        
        const double* data = waveform.data();
      
        
        DataSet dataset = group.createDataSet(waveform.name(), PredType::NATIVE_DOUBLE, dataspace);
        dataset.write(data, PredType::NATIVE_DOUBLE);

        
        addStringAttribute(dataset, "xunit", waveform.x_unit());
        addStringAttribute(dataset, "yunit", waveform.y_unit());
       
        group.close();
        
        bOk = true;
    }
    
    // catch failure caused by the H5File operations
    catch(FileIException error)
    {
        error.printError();
        bOk = false;
    }
    // catch failure caused by the DataSet operations
    catch(DataSetIException error)
    {
        error.printError();
        bOk = false;;
    }
    // catch failure caused by the DataSpace operations
    catch(DataSpaceIException error)
    {
        error.printError();
        bOk = false;
    }
    return bOk;
}

///////////////////////////////////////////////////////////////////////////////

void readDataSet(const std::string& fname)
{
    H5File file( fname, H5F_ACC_RDONLY);
    
   
    for(size_t i=0; i<file.getNumObjs( ); i++)
    {
        H5std_string groupName = file.getObjnameByIdx(i);
        fprintf(stdout, "[%ld] %s\n", i, s.c_str());
    
        
        H5G_obj_t obj = file.getObjTypeByIdx(i);
        
    }
}


///////////////////////////////////////////////////////////////////////////////

class Options
{
////////////////////////////////////////////////////////////////////////////////
    public:
////////////////////////////////////////////////////////////////////////////////

    enum Command
    {
            Command_None
        ,   Command_New
        ,   Command_Add
        ,   Command_Read
    };
    
    
    Options( )
    {
        setDefault( );
    }

    Options(const int argc, const char** argv)
    {
        setDefault( );
        parse(argc, argv);
    }

    Options::Command command( )
    { return m_command; }
    
    bool verbose( )const
    { return m_verbose; }
    

    const std::string& dbase( )const
    { return m_dbase; }
    
    const std::string& signalFile( )const
    { return m_signalFile; }
    
    const std::string group( )const
    { return m_group; }

    bool useTestSig( )const
    {  return m_testSigSize > 0 ? true : false; }
    
    size_t testSigSize( )const
    { return m_testSigSize; }
    
    void help( )const
    {
        fprintf(stdout, "%s: usage -dbase <fileName> \n", m_appname.c_str());
        fprintf(stdout,
                        "    -dbase <fileName>   The name of the hdf5 file to be created\n"
                        "    -new                Create the hdf5 file.\n"
                        "    -add                Adds data to current dbase\n"
                        "    -group  <groupName> The group to add data\n"
                        "    -signal <sigName>   The signal name\n"
                        "    -testsig <size>     Creates a test sig of given size\n"
                        "    -V                  verbose mode\n"
                        "    -h                  help\n"
                        "\n\n");

        fprintf(stdout,
            "Examples:\n"
            "%s -dbase <file> -new          # creates a new hdf5 file\n"
            "%s -dbase <file> -add -group <groupName> -signal <sigFile>  # Adds signal in sigFile to the given group\n"
            "%s -base  <file> -read"
            , m_appname.c_str()
            , m_appname.c_str()
            , m_appname.c_str()
        );

    }
    
////////////////////////////////////////////////////////////////////////////////
    private:
////////////////////////////////////////////////////////////////////////////////

    void setDefault( )
    {
        m_verbose = false;
        m_dbase = "";
        m_group   = "";
        m_signalFile = "";
        m_testSigSize = 0;
        m_command = Command_None;
    }
    void parse(const int argc, const char** argv)
    {
        m_appname = argv[0];
        if(argc == 1)
        {
            //help();
            return;
        }
        for(int i=0; i<argc; i++)
        {
            m_list.push_back(argv[i]);
        }
        parse( );
    }
    ////////////////////////////////////////////////////////////////////////////
    void parse( )
    {
        const size_t nbarg = m_list.size();
        for(size_t i=1; i<nbarg; i++)
        {
            const std::string arg  = m_list[i];
            const std::string arg1 = (i<nbarg-1 ? m_list[i+1] : std::string());
            //fprintf(stdout, "[%ld] %s %s\n", i, arg.c_str(), arg1.c_str());
            if(0) { }
            else if(arg == "-h")        help( );
            else if(arg == "-V")        m_verbose = true;
            ////////////////////////////////////////////////////////////////////
            // specific options
            else if(arg == "-dbase")    m_dbase        = arg1;
            else if(arg == "-group")    m_group        = arg1;
            else if(arg == "-signal")   m_signalFile   = arg1;
            else if(arg == "-testsig")  m_testSigSize  = atoi(arg1.c_str());
            else if(arg == "-new")      m_command = Options::Command_New;
            else if(arg == "-add")      m_command = Options::Command_Add;
            else if(arg == "-read")     m_command = Options::Command_Read;
        }
    }
    ////////////////////////////////////////////////////////////////////////////
    // variables
    std::string                  m_appname                                     ;
    std::vector<std::string>     m_list                                        ;
    bool                         m_verbose                                     ;
    std::string                  m_dbase                                       ;
    std::string                  m_group                                       ;
    std::string                  m_signalFile                                  ;
    size_t                       m_testSigSize                                 ;
    Options::Command             m_command                                     ;
  
};


////////////////////////////////////////////////////////////////////////////////

int main(const int argc, const char** argv)
{
    Options options(argc, argv);

    switch(options.command())
    {
        case Options::Command_None:
            options.help();
            break;
        case Options::Command_New:
            createNewFile(options.dbase());
            break;
        case Options::Command_Add:
            {
                Waveform wave;
                
                if(options.useTestSig())
                {
                    for(int i=0; i<options.testSigSize(); i++)
                    {
                        wave.addPoint((double)i, (double)2*i);
                    }
                    wave.m_name = options.signalFile();
                    wave.m_xunit = "s";
                    wave.m_yunit = "V";
                }
                else
                {
                    readWaveform(wave, options.signalFile());
                }
                if(wave.size() > 0)
                {
                    createDataSet(options.dbase(),
                                  options.group(),
                                  wave);
                }
            }
            break;
        case Options::Command_Read:
            readDataSet(options.dbase());
            break;
    };
    return 0;
}

           
////////////////////////////////////////////////////////////////////////////////

    

