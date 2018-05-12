#include <dirent.h>
#include <errno.h>
#include "disk_info.h"
#include "FolderInfo.h"
#include <iostream>
#include "log.h"
#include <sstream>
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

int is_folder_exist(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
        return 0;
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
        return 1;
    else
    {
        std::cerr << "Not folder: " << path << std::endl;
        return -EINVAL;
    }
}

eFileType get_file_type(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        log(LEVEL_ERROR, "Main", "Stat Failed: %s", path.c_str());
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
    }
}

bool is_media_folder(FolderInfo &info)
{
    if (info.filename_cue.size() == 0 && info.filename_music.size() == 0)
    {
        return false;
    }
    /*
    std::cout << "Folder: " << info.root_path << std::endl;
    std::cout << "\r - Num of cues: " << info.filename_cue.size() << std::endl;
    std::cout << "\r - Num of medias: " << info.filename_music.size() << std::endl;
    */
    return true;
}

int clean_folder(const std::string &src_path)
{
    DIR *dir;
    struct dirent *ptr;
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        std::cout << "Dir open err: " << src_path << std::endl;
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        filename = src_path + "/" + ptr->d_name;
        eFileType type = get_file_type(filename);
        if (type == eFileType_Folder)
        {
            ret = clean_folder(filename);
        }
        else
        {
            ret = remove(filename.c_str());
        }
        if (ret != 0)
        {
            std::cerr << "Failed remove: " << filename << std::endl;
            break;
        }
    }
out:
    closedir(dir);
    return ret;
}

int create_folder(const std::string &src_path)
{
    std::istringstream iss(src_path);
    std::string s, path_;
    int ret;

    while(getline(iss, s, '/' ))
    {
        if (s.size() == 0)
            continue;
        path_ += "/" + s;
        ret = is_folder_exist(path_);
        if (ret == 1)
            continue;
        else if (ret < 0)
        {
            std::cerr << "Invalid folder: " << path_ << std::endl;
            return ret;
        }
        else
        {
            ret = mkdir(path_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if (ret != 0)
            {
                std::cerr << "Failed create folder: " << path_ << std::endl;
                return ret;
            }
        }
    }
    return 0;
}

int make_clean_folder(std::string &path)
{
    struct stat sb;
    int ret = 0;

    if (stat(path.c_str(), &sb) == 0) //already exists
    {
        ret = clean_folder(path);
        if (ret)
            return ret;
    }
    ret = create_folder(path);
    return ret;
}

int convert_cue_files(const std::string &dest_root,
        DiscInfo &disk_info,
        FolderInfo &folder_info,
        const std::string &cue_file_name)
{
    std::string work_root = dest_root + "/" +
        folder_info.root_path + "/" +
        cue_file_name;
    int ret;

    //std::cout << "Preparing Folder: " << work_root << std::endl;
    ret = make_clean_folder(work_root);
    if (ret != 0)
    {
        return ret;
    }

    for (int i = 0; i < disk_info.tracks.size(); i++)
    {
        TrackInfo *track_info = &disk_info.tracks[i];
        //std::string media_file = folder_info.root_path + "/" + track_info->file;
        std::string media_file = track_info->file;
        //std::cout << media_file << std::endl;
        //TODO: Check file existen, and start convert
    }
}

int process_dir(const std::string &src_path, const std::string &dst_path)
{
    DIR *dir;
    struct dirent *ptr;
    FolderInfo folder_info(src_path);

    //std::cout << "Processing Folder: " << path << std::endl;
    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        std::cout << "Dir open err: " << src_path << std::endl;
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        eFileType type = get_file_type(src_path + "/" + ptr->d_name);
        switch (type)
        {
            case eFileType_Media:
                folder_info.add_media(filename);
                break;
            case eFileType_Cue:
                folder_info.add_cue(filename);
                break;
            case eFileType_Folder:
                process_dir(src_path + "/" + ptr->d_name, dst_path);
                break;
            default:
                break;
        }
    }
    closedir(dir);

    if (is_media_folder(folder_info) == false)
    {
        return 0;
    }

    /*
     * First Process Cue File.
     * The remain media files will be converted in whole
     */
    for (auto cue_index = folder_info.filename_cue.begin();
            cue_index != folder_info.filename_cue.end();
            cue_index++)
    {
        DiscInfo disc_info;
        std::string full_path = folder_info.root_path + "/" + *cue_index;

        //std::cout << "Processing " << full_path << std::endl;
        log(LEVEL_INFO, "Main", "Processing: %s", full_path.c_str());

        disc_info.parse_file(full_path);

        convert_cue_files(dst_path, disc_info, folder_info, (*cue_index).substr(
                    0, (*cue_index).find_last_of('.')));
    }
    /*
     * Process remaining Media Files
     */
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
    process_dir(src_folder, dst_folder);
    return 0;
}
