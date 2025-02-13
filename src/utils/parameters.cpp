#include "parameters.h"
#include "utils.h"

Parameters::Parameters(int argc, const char *argv[])
{
    if (argc <= 1)
    {
        log() << "Too few args..." << std::endl;
        exit(1);
    }
    for (int i = 1; i < argc; i++)
    {
        // if (strcmp(argv[i], "-cap") == 0)
        // {
        //     cap_file = argv[++i];
        // }
        // else if (strcmp(argv[i], "-net") == 0)
        // {
        //     net_file = argv[++i];
        // }
        // else if (strcmp(argv[i], "-output") == 0)
        // {
        //     out_file = argv[++i];
        // }
        // else if (strcmp(argv[i], "-threads") == 0)
        // {
        //     threads = std::stoi(argv[++i]);
        // }else if (strcmp(argv[i], "-v") == 0)
        // {
        //     v = argv[++i];
        // }
        // else
        // {
        //     log() << "Unrecognized arg..." << std::endl;
        //     log() << argv[i] << std::endl;
        // }

        if (strcmp(argv[i], "-def") == 0)
        {
            def_file = argv[++i];
        }
        else if (strcmp(argv[i], "-v") == 0)
        {
            v_file = argv[++i];
        }
        else if (strcmp(argv[i], "-sdc") == 0)
        {
            sdc_file = argv[++i];
        }
        else if (strcmp(argv[i], "-cap") == 0)
        {
            cap_file = argv[++i];
        }
        else if (strcmp(argv[i], "-net") == 0)
        {
            net_file = argv[++i];
        }
        else if (strcmp(argv[i], "-output") == 0)
        {
            out_file = argv[++i];
        }else if (strcmp(argv[i], "-area_threads") == 0)
        {
            area_threads = std::stoi(argv[++i]);
        }else if (strcmp(argv[i], "-inner_threads") == 0)
        {
            inner_threads = std::stoi(argv[++i]);
        }
        else
        {
            log() << "Unrecognized arg..." << std::endl;
            log() << argv[i] << std::endl;
        }
    }
    log() << "cap file: " << cap_file << std::endl;
    log() << "net file: " << net_file << std::endl;
    log() << "output  : " << out_file << std::endl;
    log() << "area_threads : " << area_threads << std::endl;
    log() << "inner_threads : " << inner_threads << std::endl;
    log() << std::endl;
}
