#ifndef BDESERVER_TYPEDEFS_H
#define BDESERVER_TYPEDEFS_H

enum class rawjob_state
{
    waiting = 0,
    transferring = 1,
    finished = 2,
    error = 3,
};

enum class sjob_state
{
    waiting = 0,
    transferring = 1,
    finished = 2,
    error = 3,

    bootstrap = 12,
    dtn_matching = 4,
    wan_setup = 5,
    sdn_setup = 6,
    path_verification = 7,
    launching = 8,
    checksum_verification = 9,
    sdn_teardown = 10,
    wan_teardown = 11,
};

enum class block_state
{
    waiting = 0,
    transferring = 1,
    finished = 2,
};

enum class flow_state
{
    scheduling = 0,
    setting = 1,
    transferring = 2,
    waiting = 3,
    waiting_to_be_teared = 4,
    tearing = 5,
    done = 6,
    error = 7,
};

// sjob stages: (general is the general state of the entire stage)
//     bootstrap: general, query, match
//     network:   general, planning, public, wan_static, wan_dynamic, lan_src, lan_dst, ping
//     transfer:  general, launch, transfer, checksum
//     teardown:  general, public, wan_dynamic, lan_src, lan_dst

enum class sjob_stage
{
    pending = 0,
    working = 1,
    success = 2,
    error = 3,
};




#endif
