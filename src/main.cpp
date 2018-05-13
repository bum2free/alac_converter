#include <dirent.h>
#include <errno.h>
#include "disk_info.h"
#include "FolderInfo.h"
#include <iostream>
#include "log.h"
#include <regex>
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
        std::cout << "--start time: " << track.start_time << std::endl;
        std::cout << "--duration: " << track.duration << std::endl;
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

int make_clean_folder(const std::string &path)
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

int convert_file(const std::string &dst_file, const std::string &src_file)
{
//    std::cout <<"src: " << src_file << std::endl;
//    std::cout <<"dst: " << dst_file << std::endl;
    int ret = 0;
    int length_filename = src_file.size() + dst_file.size();

    char *cmd = nullptr;
    try {
        cmd = new char[length_filename + 1024];

        sprintf(cmd, "ffmpeg -i %s -acodec alac %s",
                src_file.c_str(),
                dst_file.c_str());
        std::cout << "cmd: " << cmd << std::endl;
        system(cmd);
    }
    catch (std::exception &e)
    {
        ret = -ENOMEM;
    }
    if (cmd != nullptr)
        delete[] cmd;
    return ret;
}
int convert_file(std::string &dst_file, std::string &src_file,
        DiscInfo &disc_info, int index)
{
    TrackInfo *track_info = &disc_info.tracks[index];
    if (track_info->start_time < 0)
    {
        return -EINVAL;
    }
    std::regex regex(" ");
    src_file = std::regex_replace(src_file, regex, "\\ ");
    dst_file = std::regex_replace(dst_file, regex, "\\ ");
    //std::cout << "src: " << src_file << std::endl;
    //std::cout << "dst: " << dst_file << std::endl;

    /*
     * metadata:
     * - track titile
     * - track performer
     * - album
     * - track
     */
    int length_metadata = track_info->title.size() +
        track_info->performer.size() + track_info->title.size();
    int length_filename = src_file.size() + dst_file.size();

    char *meta_buf = nullptr;
    char *cmd = nullptr;
    int ret = 0;
    try {
        meta_buf = new char[length_metadata + 128];
        cmd = new char[length_filename + 1024];

        sprintf(meta_buf, "-metadata album=\"%s\" -metadata artist=\"%s\" "
                "-metadata title=\"%s\" -metadata track=%d",
                disc_info.title.c_str(),
                track_info->performer.c_str(),
                track_info->title.c_str(),
                index);
        if (track_info->duration > 0)
        {
            sprintf(cmd, "ffmpeg -ss %d -i %s -t %d %s -acodec alac %s",
                    track_info->start_time,
                    src_file.c_str(),
                    track_info->duration,
                    meta_buf,
                    dst_file.c_str());
        }
        else
        {
            sprintf(cmd, "ffmpeg -ss %d -i %s %s -acodec alac %s",
                    track_info->start_time,
                    src_file.c_str(),
                    meta_buf,
                    dst_file.c_str());
        }
        std::cout << "cmd: " << cmd << std::endl;
        system(cmd);
    }
    catch (std::exception &e)
    {
        ret = -ENOMEM;
    }
    if (meta_buf != nullptr)
        delete[] meta_buf;
    if (cmd != nullptr)
        delete[] cmd;
    return ret;
}

int convert_cue_files(const std::string &work_root,
        DiscInfo &disk_info,
        FolderInfo &folder_info)
{
    std::set<std::string> files_used;
    int ret;

    //std::cout << "Preparing Folder: " << work_root << std::endl;
    ret = make_clean_folder(work_root);
    if (ret != 0)
    {
        return ret;
    }
    //print_cd_info(&disk_info);
    for (int i = 0; i < disk_info.tracks.size(); i++)
    {
        TrackInfo *track_info = &disk_info.tracks[i];
        std::string src_file = folder_info.root_path + "/" + track_info->file;
        std::string dest_file = work_root + "/" + std::to_string(i + 1) + "_" + track_info->title + ".m4a";
        //std::cout << "src: " << src_file << std::endl;
        //std::cout << "dst: " << dest_file << std::endl;
        //TODO: Check file existen, and start convert
        convert_file(dest_file, src_file, disk_info, i);
        files_used.insert(track_info->file);
    }
    /*
     * remove used files
     * This is for processing individual media files which does not contained
     * in the cue files
     */
    for (auto i = files_used.begin(); i!= files_used.end(); i++)
    {
        folder_info.filename_music.erase(*i);
    }
}

int process_dir(const std::string &src_path, const std::string &dst_path)
{
    DIR *dir;
    struct dirent *ptr;
    FolderInfo folder_info(src_path);
    int ret = 0;

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

    ret = make_clean_folder(dst_path + "/" + folder_info.root_path);
    if (ret != 0)
    {
        return ret;
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

        convert_cue_files(dst_path + "/" + folder_info.root_path + "/" +
                (*cue_index).substr(0, (*cue_index).find_last_of('.')),
                disc_info, folder_info);
    }
    /*
     * Process remaining Media Files
     */
    for (auto i = folder_info.filename_music.begin(); i != folder_info.filename_music.end(); i++)
    {
        std::string src_file = folder_info.root_path + "/" + *i;
        convert_file(dst_path + "/" + folder_info.root_path + "/" +
                (*i).substr(0, (*i).find_last_of('.')) + ".m4a",
                    src_file);

    }
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
