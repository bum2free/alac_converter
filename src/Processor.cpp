#include <dirent.h>
#include "disk_info.h"
#include <errno.h>
#include "FolderInfo.h"
#include <fstream>
#include <iostream>
#include "log.h"
#include "Processor.h"
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

extern Logger logger;

static const char *ffmpeg_log_level = "error";

Processor::Processor(const std::string &src_path, const std::string &dst_path) :
    src_path_(src_path), dst_path_(dst_path)
{
}

int Processor::process(void)
{
    return process_dir(src_path_, dst_path_);
}

int Processor::process_dir(const std::string &src_path, const std::string &dst_path)
{
    DIR *dir;
    struct dirent *ptr;
    FolderInfo folder_info(src_path);
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        logger(LEVEL_ERROR, "Dir open err: %s", src_path.c_str());
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

    if (folder_info.is_media_folder() == false)
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
        logger(LEVEL_INFO, "Processing: %s", full_path.c_str());

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

int Processor::is_folder_exist(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
        return 0;
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
        return 1;
    else
    {
        logger(LEVEL_ERROR, "Not folder: %s", path.c_str());
        return -EINVAL;
    }
}

eFileType Processor::get_file_type(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        logger(LEVEL_ERROR, "Stat Failed: %s", path.c_str());
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

int Processor::clean_folder(const std::string &src_path)
{
    DIR *dir;
    struct dirent *ptr;
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        logger(LEVEL_ERROR, "Dir open err: %s", src_path.c_str());
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
            logger(LEVEL_ERROR, "Failed remove: %s", filename.c_str());
            break;
        }
    }
out:
    closedir(dir);
    return ret;
}

int Processor::create_folder(const std::string &src_path)
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
            logger(LEVEL_ERROR, "Invalid folder: %s", path_.c_str());
            return ret;
        }
        else
        {
            ret = mkdir(path_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if (ret != 0)
            {
                logger(LEVEL_ERROR, "Failed create folder: %s", path_);
                return ret;
            }
        }
    }
    return 0;
}

int Processor::make_clean_folder(const std::string &path)
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

int Processor::convert_file(const std::string &dst_file, const std::string &src_file)
{
//    std::cout <<"src: " << src_file << std::endl;
//    std::cout <<"dst: " << dst_file << std::endl;
    int ret = 0;

    std::vector<std::string> cmd_args_;
    cmd_args_.push_back("ffmpeg");
    //set log level
    cmd_args_.push_back("-loglevel");
    cmd_args_.push_back(ffmpeg_log_level);
    //set input file
    cmd_args_.push_back("-i");
    cmd_args_.push_back(src_file);
    //set codec format
    cmd_args_.push_back("-acodec");
    cmd_args_.push_back("alac");
    //set dst file
    cmd_args_.push_back(dst_file);
    logger(LEVEL_INFO, "Processing: %s", dst_file.c_str());
    ret = exec_cmd("ffmpeg", cmd_args_);
    if (ret)
        logger(LEVEL_ERROR, "!Error");

    return ret;
}

int Processor::convert_file(std::string &dst_file, std::string &src_file,
        DiscInfo &disc_info, int index)
{
    TrackInfo *track_info = &disc_info.tracks[index];
    int ret = 0;
    if (track_info->start_time < 0)
    {
        return -EINVAL;
    }
    //std::cout << "src: " << src_file << std::endl;
    //std::cout << "dst: " << dst_file << std::endl;

    std::vector<std::string> cmd_args_;
    cmd_args_.push_back("ffmpeg");
    //set log level
    cmd_args_.push_back("-loglevel");
    cmd_args_.push_back(ffmpeg_log_level);
    //set start time
    cmd_args_.push_back("-ss");
    cmd_args_.push_back(std::to_string(track_info->start_time));
    //set input file
    cmd_args_.push_back("-i");
    cmd_args_.push_back(src_file);
    //set end time
    if (track_info->duration > 0)
    {
        cmd_args_.push_back("-t");
        cmd_args_.push_back(std::to_string(track_info->duration));
    }
    //set codec format
    cmd_args_.push_back("-acodec");
    cmd_args_.push_back("alac");
    //set metadata
    cmd_args_.push_back("-metadata");
    //cmd_args_.push_back(std::string("album=\"") + disc_info.title + "\"");
    cmd_args_.push_back(std::string("album=") + disc_info.title);
    cmd_args_.push_back("-metadata");
    cmd_args_.push_back(std::string("artist=") + track_info->performer);
    cmd_args_.push_back("-metadata");
    cmd_args_.push_back(std::string("title=") + track_info->title);
    cmd_args_.push_back("-metadata");
    cmd_args_.push_back(std::string("track=") + std::to_string(index + 1));

    //set dst file
    cmd_args_.push_back(dst_file);

    logger(LEVEL_INFO, "Processing: %s", dst_file.c_str());
    ret = exec_cmd("ffmpeg", cmd_args_);
    if (ret)
        logger(LEVEL_ERROR, "!Error");
    return ret;
}

int Processor::convert_cue_files(const std::string &work_root,
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
    if (disk_info.disc_id.size() != 0)
    {
        write_disc_id(work_root, disk_info.disc_id);
    }

    for (int i = 0; i < disk_info.tracks.size(); i++)
    {
        TrackInfo *track_info = &disk_info.tracks[i];
        std::string src_file = folder_info.root_path + "/" + track_info->file;
        std::string title = std::regex_replace(track_info->title, std::regex("/"), "-");
        std::string dest_file = work_root + "/" + std::to_string(i + 1) + "_" + title + ".m4a";
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

int Processor::exec_cmd(const char *name, std::vector<std::string> &cmd_args_)
{
	pid_t pid;
    int ret = 0;

    const char *cmd_args[cmd_args_.size() + 1];
    //cmd_args = new const char*[cmd_args_.size() + 1];
    for (int i = 0; i < cmd_args_.size(); i++)
    {
        cmd_args[i] = cmd_args_[i].c_str();
    }
    cmd_args[cmd_args_.size()] = NULL;

	pid = fork();
    if (pid == -1)
        return -ENOMEM;

    else if (pid != 0 )
    {//parent
		pid_t child_pid;
		int stat_val;
		/*
		 * WIFEXITED(stat_val): Non-zero if the child is terminated
		 * normally.
		 *
		 * WEXITSTATUS(stat_val): if WIFEXITED is non-zero, this
		 * returns child exit code.
		 *
		 * WIFSIGNALED(stat_val): Non-zero if the child is terminated
		 * on an uncaught signal.
		 *
		 * WTERMSIG(stat_val): if WIFSIGNALED is non-zero, this
		 * returns a signal number.
		 *
		 * WIFSTOPPED(stat_val): Non-zero if the child has stopped.
		 *
		 * WSTOPSIG(stat_val): if WIFSTOPPED is non-zero, this returns
		 * a signal number.
		 */
        child_pid = waitpid(pid, &stat_val, 0);
        if (child_pid == -1) {
            /*
             * TBD: check ther errno.
             *
             * Could be:
             * ECHILD: no child process;
             * EINTR: interrupted by signal;
             * EINVAL: invalid option argument
             */
        }
		if (WIFEXITED(stat_val)) {
			//printf("Child exited with code:%d\n", WEXITSTATUS(stat_val));
        } else if(WIFSIGNALED(stat_val)) {
			printf("Child interrupted by signal: %d\n", WTERMSIG(stat_val));
            ret = -EINVAL;
        } else {
			printf("Child stopped unknown\n");
            ret = -EINVAL;
        }
	} else {
		/* This is the child */
        return execvp(name, (char **)cmd_args);
	}
	return ret;
}

int Processor::write_disc_id(const std::string &folder, const std::string &id)
{
    std::ofstream out_file;
    out_file.open(folder + "/" + "disc_id.txt");

    if (out_file.is_open() == false)
        return -EINVAL;

    logger(LEVEL_INFO, "Write disk_id: %s", id.c_str());
    out_file << id;
    out_file.close();
    return 0;
}
