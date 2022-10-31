#ifndef BDESERVER_JOBMANAGER_H
#define BDESERVER_JOBMANAGER_H

#include <string>
#include <map>

enum class JobState
{
    running,
    stopped,
    finished,

    reserved,
    marked_for_prealloc,
};

enum class JobGroup
{
    base,
    extra
};

// DTN to DTN job
class BaseJob
{
public:
    BaseJob(std::string const & id, JobGroup jg, JobState js, double r)
    : id_(id), jg_(jg), js_(js), rate_(r)
    { }

    std::string const & id() const
    { return id_; }

    JobGroup group() const
    { return jg_; }

    JobState state() const
    { return js_; }

    double rate() const
    { return rate_; }

    void set_state(JobState s)
    { js_ = s; }

private:

    std::string id_;

    JobGroup jg_;
    JobState js_;
    double rate_;

    std::string storage;
    std::string dtn;
};

// user submitted job
class Job
{
};


class JobManager
{
public:

    JobManager();

    Job       & get_job(std::string const & id);
    Job const & get_job(std::string const & id) const;

    BaseJob       & get_basic_job(std::string const & id)
    { return basic_jobs.find(id)->second; }

    BaseJob const & get_basic_job(std::string const & id) const
    { return basic_jobs.find(id)->second; }

    BaseJob & add_basic_job(std::string const & id, JobGroup jg, JobState js, double r)
    { 
        //BaseJob job(id, jg, js, r);
        //return basic_jobs.insert(std::make_pair(id, job));
        return basic_jobs.emplace(id, BaseJob{id, jg, js, r}).first->second;
    }

private:

    std::map<std::string, Job> jobs;
    std::map<std::string, BaseJob> basic_jobs;

};

#endif
