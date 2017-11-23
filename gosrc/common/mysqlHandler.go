package common

import (
	"database/sql"
	"errors"
	"fmt"
	_ "github.com/go-sql-driver/mysql"
)

type MysqlClient struct {
	Conn []*sql.DB
}

type MysqlServer struct {
	Group []*MysqlClient
}

func (s *MysqlServer) InitDB(cfg *Config, max int, min int) error {
	for i := range cfg.Dbs {
		c := &MysqlClient{}
		for j := range cfg.Dbs[i].Urls {
			db, er := sql.Open("mysql", cfg.Dbs[i].Urls[j])
			if er != nil {
				return er
			}
			db.SetMaxOpenConns(max)
			db.SetMaxIdleConns(min)
			c.Conn = append(c.Conn, db)
		}
		s.Group = append(s.Group, c)
	}
	return nil
}

func (s *MysqlServer) Query(sql string, id int) ([]map[string]string, error) {
	if id >= len(s.Group) || id < 0 {
		return nil, errors.New("msyql query false, id error")
	}
	for _, value := range s.Group[id].Conn {
		rows, er := value.Query(sql)
		if er != nil {
			continue
		}
		c, er := rows.Columns()
		if er != nil {
			return nil, er
		}
		scanArgs := make([]interface{}, len(c))
		values := make([]interface{}, len(c))
		for j := range values {
			scanArgs[j] = &values[j]
		}
		var r []map[string]string
		for rows.Next() {
			record := make(map[string]string)
			err := rows.Scan(scanArgs...)
			if err != nil {
				break
			}
			for i, col := range values {
				if col != nil {
					record[c[i]] = string(col.([]byte))
				}
			}
			r = append(r, record)
		}
		return r, nil
	}
	return nil, errors.New("msyql query false")
}

func GetIndex(id int, cnt uint) uint {
	bstr := []byte(fmt.Sprintf("%d", id))
	r := uint(0)
	for _, v := range bstr {
		r = (r*131 + uint(v)) % 4294967296
	}
	return r % cnt
}
