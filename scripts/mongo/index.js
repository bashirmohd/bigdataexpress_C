
var MongoClient = require("mongodb").MongoClient;
var assert = require("assert");

var url = 'mongodb://localhost:27017/bde'

MongoClient.connect(url, function(err, db) {
    if (err) {
        console.log(err);
        return;
    }

    console.log("connected");

    db.collection("rawjob").removeMany();
    db.collection("sjob").removeMany();
    db.collection("block").removeMany();

    console.log("all removed");

    db.close();
});
