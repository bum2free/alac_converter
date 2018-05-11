#include <dirent.h>
#include <errno.h>
#include "FolderInfo.h"
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <unistd.h>

typedef enum
{
    eFileType_None,
    eFileType_Folder,
    eFileType_Media,
    eFileType_Cue,
    eFileType_Unknown,
    eFileType_UnSupported,
}eFileType;

eFileType get_file_type(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        std::cout << "Stat Failed: " << path << std::endl;
        return eFileType_None;
    }
    switch (sb.st_mode & S_IFMT)
    {
        case S_IFDIR:
            return eFileType_Folder;
        case S_IFREG:
        {
            int index = path.find_last_of('.');
            std::string file_type = path.substr(index + 1, path.size() - index);
            if (file_type == "ape" || file_type == "flac" || file_type == "wav")
            {
                return eFileType_Media;
            }
            else if (file_type == "cue")
            {
                return eFileType_Cue;
            }
            else
            {
                return eFileType_UnSupported;
            }
        }
        default:
            return eFileType_Unknown;
    }
}

void print_folder_info(FolderInfo &info)
{
    if (info.filename_cue.size() == 0 && info.filename_music.size() == 0)
    {
        return;
    }
    std::cout << "Folder: " << info.root_path << std::endl;
    std::cout << "\r - Num of cues: " << info.filename_cue.size() << std::endl;
    std::cout << "\r - Num of medias: " << info.filename_music.size() << std::endl;
}

int process_dir(const std::string &path)
{
    DIR *dir;
    struct dirent *ptr;
    FolderInfo folder_info(path);

    //std::cout << "Processing Folder: " << path << std::endl;
    dir = opendir(path.c_str());
    if (dir == nullptr)
    {
        std::cout << "Dir open err: " << path << std::endl;
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        eFileType type = get_file_type(path + "/" + ptr->d_name);
        switch (type)
        {
            case eFileType_Media:
                folder_info.add_media(filename);
                break;
            case eFileType_Cue:
                folder_info.add_cue(filename);
                break;
            case eFileType_Folder:
                process_dir(path + "/" + ptr->d_name);
                break;
            default:
                break;
        }
    }
    closedir(dir);
    //Test
    print_folder_info(folder_info);
    return 0;
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
        std::cout << "Err: src or dest dir not set" << std::endl;
        std::cout << "Usage: alac_convert -s <src_dir> -d <dst_dir>" << std::endl;
        return -EINVAL;
    }
    std::cout << "Src Folder: " << src_folder << std::endl;
    std::cout << "Dst Folder: " << dst_folder << std::endl;

    //Test
    process_dir(src_folder);
    return 0;
}
