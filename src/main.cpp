#include "FolderInfo.h"
#include <iostream>
#include "log.h"
#include "Processor.h"

Logger logger(2048);

void print_cd_info(DiscInfo *disk)
{
    std::cout << "Disk Title: " << disk->title << std::endl;
    std::cout << "Disk Performer: " << disk->performer << std::endl;
    std::cout << "Num of tracks: " << disk->tracks.size() << std::endl;
    std::cout << "Track Info:" << std::endl;
    for (auto i = disk->tracks.begin(); i != disk->tracks.end(); i++)
    {
        auto track = *i;
        std::cout << "--Track: " << track.index << std::endl;
        std::cout << "\rtitle: " << track.title << std::endl;
        std::cout << "\rperformer: " << track.performer << std::endl;
        std::cout << "\rfile: " << track.file << std::endl;
        for (auto j = track.time_index.begin();
                j != track.time_index.end(); j++)
        {
            std::cout << "\rtime index: " << *j << std::endl;
        }
        std::cout << "--start time: " << track.start_time << std::endl;
        std::cout << "--duration: " << track.duration << std::endl;
    }
}

int main(int argc, char *argv[])
{
    int c;
    std::string src_folder, dst_folder;

    while ((c = getopt(argc, argv, "s:d:")) != -1)
    {
        switch (c)
        {
            case 's':
                src_folder = optarg;
                break;
            case 'd':
                dst_folder = optarg;
                break;
            case '?':
                return -EINVAL;
            default:
                return -1;
        }
    }

    if (src_folder.size() == 0 || dst_folder.size() == 0)
    {
        logger(LEVEL_ERROR, "src or dest dir not set");
        logger(LEVEL_INFO, "Usage: alac_convert -s <src_dir> -d <dst_dir>");
        return -EINVAL;
    }

    logger(LEVEL_INFO, "Src Folder: %s", src_folder.c_str());
    //std::cout << "Src Folder: " << src_folder << std::endl;
    std::cout << "Dst Folder: " << dst_folder << std::endl;

    logger.set_log_err_file(dst_folder + "/" + "error.log");
    //Test
    Processor processor(src_folder, dst_folder);
    processor.process();
    return 0;
}
