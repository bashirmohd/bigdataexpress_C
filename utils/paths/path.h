#ifndef BDE_UTILS_PATH_H
#define BDE_UTILS_PATH_H

namespace utils
{
    enum class PathType
    {
        Error,
        DoesNotExist,
        RegularFile,
        Directory,
        SymbolicLink,
        HardLink,
        Fifo,
        CharacterFile,
        BlockFile,
        Socket,
        Unknown
    };

    class Path
    {
    public:
        explicit Path(const std::string &p)
            : path_(p) {
            init();
        }

        const std::string& name() const { return path_; }
        size_t   size() const           { return size_; }
        PathType type() const           { return type_; }
        uid_t    uid() const            { return uid_ ; }
        gid_t    gid() const            { return gid_ ; }
        gid_t    mode() const           { return mode_; }

        bool readable_by(uid_t uid, gid_t gid) const;
        bool readable_by(std::string const & user) const;

        bool ok() const; // false if stat() errored.

        bool exists() const;
        bool is_directory() const;
        bool is_regular_file() const;
        bool is_symbolic_link() const;

        const Path follow_sybolic_link() const;

        const std::string canonical_name() const;  // realpath(3) wrapper.
        const std::string base_name() const;       // basename(3) wrapper.
        const std::string dir_name() const;        // dirname(3) wrapper.

        std::string join(std::string const &other) const;

        bool operator==(const Path &b) const { return path_ == b.path_; }
        bool operator!=(const Path &b) const { return path_ != b.path_; }

        bool operator<(const Path &b) const { return name() < b.name(); }

    private:
        void init();

    private:
        std::string       path_;
        PathType          type_;
        size_t            size_;
        uid_t             uid_;
        gid_t             gid_;
        mode_t            mode_;
    };

    // Join two path names, with the standard UNIX path separator
    // in between.  This got to be fairly involved than simple
    // string concatenation, since we like things to look pretty.
    std::string join_paths(std::string const & a, std::string const & b);
};

std::ostream& operator<<(std::ostream& out, const utils::PathType& type);

#endif // BDE_UTILS_PATH_H

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:
