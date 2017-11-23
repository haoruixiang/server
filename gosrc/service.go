package main

import (
	"./common"
	"database/sql"
	"log"
)

type Relation struct {
	Conf common.Config
	Db   *common.MysqlServer
}

func (r *Relation) Init() {
	for i := range r.Conf.Dbs {
		var c common.MysqlClient
		for j := range r.Conf.Dbs[i].Urls {
			db, err := sql.Open("mysql", r.Conf.Dbs[i].Urls[j])
			if err != nil {
				log.Println("ERROR", err)
				continue
			}
			db.SetMaxOpenConns(1024)
			db.SetMaxIdleConns(128)
			c.Conn = append(c.Conn, db)
		}
		r.Db.Group = append(r.Db.Group, &c)
	}
}

func (r *Relation) GetLoreField(){
}

func (r *Relation) AddLoreProint() {
}
func (r *Relation) DelLoreProint() {
}
func (r *Relation) GetLoreInfo() {
}
func (r *Relation) GetUserRelation() {
}
func (r *Relation) GetLoreProintList() {
}
func (r *Relation) GetLoreProintListLike() {
}
func (r *Relation) AddUserRelation() {
}
func (r *Relation) DelUserRelation() {
}
