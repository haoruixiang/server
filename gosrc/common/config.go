package common

type DbGroup struct {
	Urls []string `json:"urls"`
}

type Config struct {
	Dbs []DbGroup `json:"dbs"`
}
