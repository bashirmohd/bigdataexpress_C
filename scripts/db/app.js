
var url = 'mongodb://localhost:27017/bde';
var mongo = require('mongodb').MongoClient;
var assert = require('assert');
var async = require('async');

var storages = [
  { id: 's-bde3', name: 'storage-local-bde3', type: 'local', used_read_bw: 5000, used_write_bw: 2000, max_read_bw: 100000, max_write_bw: 800000},
  { id: 's-bde5', name: 'storage-local-bde5', type: 'local', used_read_bw: 5000, used_write_bw: 2000, max_read_bw: 100000, max_write_bw: 800000 }
];


var dtns = [
  { host: 'bde3.fnal.gov', bwin: 10000, bwout: 10000 },
  { host: 'bde-hp5.fnal.gov', bwin: 20000, bwout: 40000 }
];

/*
var sdmaps = {
  's-bde3': 'bde3.fnal.gov',
  's-bde5': 'bde-hp5.fnal.gov'
};
*/

var sdmaps = [
  { storage: 's-bde3', dtn: 'bde3.fnal.gov' },
  { storage: 's-bde5', dtn: 'bde-hp5.fnal.gov' }
];

async.waterfall([
  function(cb) {
    mongo.connect(url, function(err, db) {
      if (err) return cb(err);
      console.log('connected');
      return cb(null, db);
    });
  },

  function(db, cb) {
    db.collection('storage').drop( function(err, res) {
      //if (err) return cb(err);
      console.log('drop storage', res);
      return cb(null, db);
    });
  },

  function(db, cb) {
    db.collection('dtn').drop( function(err, res) {
      //if (err) return cb(err);
      console.log('drop dtn', res);
      return cb(null, db);
    });
  },

  function(db, cb) {
    db.collection('sdmap').drop( function(err, res) {
      //if (err) return cb(err);
      console.log('drop storage to dtn map', res);
      return cb(null, db);
    });
  },

  function(db, cb) {
    db.collection('storage').insert(storages, function(err, r1) {
      if (err) return cb(err);
      console.log(r1);
      return cb(null, db, r1);
    });
  },

  function(db, sr1, cb) {
    db.collection('dtn').insert(dtns, function(err, r2) {
      if (err) return cb(err);
      console.log(r2);
      return cb(null, db, sr1, r2);
    });
  },

  function(db, sr1, dr1, cb) {

 /*
    var sr = sr1.ops;
    var dr = dr1.ops;

    var s = {};
    var d = {};

    for (var i=0; i<sr.length; ++i) {
      s[sr[i].id] = sr[i]._id;
    }

    for (var i=0; i<dr.length; ++i) {
      d[dr[i].host] = dr[i]._id;
    }

    var sd = [];

    for (var sid in sdmaps) {
      sd.push({storage: s[sid], dtn: d[sdmaps[sid]]});
    }

    //console.log(s);
    //console.log(d);
    //console.log(sd);
*/

    db.collection('sdmap').insert(sdmaps, function(err, r3) {
      if (err) return cb(err);
      console.log(r3);
      return cb(null, db);
    });
  },

  function(db, cb) {
    db.close();
    return cb(null);
  }
], function(err) {
  if (err) return console.log(err);

  console.log('finished');
});


