package main

import (
	"fmt"
	"log"
	"os/exec"
	"sort"
	"strconv"
	"strings"
	"sync"
)

type Parameter struct {
	I         float64
	J         float64
	K1        float64
	K2        float64
	Cost1     int
	Cost2     int
	TotalCost int
	C         float64
	M         float64
}

func New(a1, a2, a3, a4, c, m float64, c1, c2, t int) Parameter {
	return Parameter{
		I:         a1,
		J:         a2,
		K1:        a3,
		K2:        a4,
		Cost1:     c1,
		Cost2:     c2,
		TotalCost: t,
		C:         c,
		M:         m,
	}
}

func main() {
	lock := sync.Mutex{}
	wg := sync.WaitGroup{}

	yearAndCpu := make([]Parameter, 0)

	i := 2.0
	j := 0.4
	k1 := 2.3
	k2 := 1.7
	cpu := 1.2
	mem := 1.4

	// for cpu := 0.1; cpu <= 2; cpu += 0.1 {
	// 	for mem := 0.1; mem <= 2; mem += 0.1 {
	wg.Add(1)
	go func(_c, _m float64) {
		weight_cpu := _c
		weight_mem := _m

		output1, err := exec.Command("./CodeCraft-2021", "0",
			fmt.Sprintf("%f", i), fmt.Sprintf("%f", j),
			fmt.Sprintf("%f", k1), fmt.Sprintf("%f", k2),
			"1000", fmt.Sprintf("%f", weight_cpu),
			fmt.Sprintf("%f", weight_mem)).Output()
		if err != nil {
			log.Fatal(err)
		}

		output2, err := exec.Command("./CodeCraft-2021", "1",
			fmt.Sprintf("%f", i), fmt.Sprintf("%f", j),
			fmt.Sprintf("%f", k1), fmt.Sprintf("%f", k2),
			"1000", fmt.Sprintf("%f", weight_cpu),
			fmt.Sprintf("%f", weight_mem)).Output()
		if err != nil {
			log.Fatal(err)
		}

		cost1, err := strconv.Atoi(strings.Trim(string(output1), "\n"))
		if err != nil {
			log.Fatal(err)
		}
		cost2, err := strconv.Atoi(strings.Trim(string(output2), "\n"))
		if err != nil {
			log.Fatal(err)
		}

		// fmt.Printf("i: %f, j: %f, k1: %f, k2: %f ", i, j, k1, k2)
		// fmt.Printf("cost: %d\t%d\t%d\n", cost1+cost2, cost1, cost2)

		lock.Lock()
		yearAndCpu = append(yearAndCpu, New(i, j, k1, k2,
			weight_cpu, weight_mem, cost1, cost2, cost1+cost2))
		lock.Unlock()

		wg.Done()
	}(cpu, mem)
	// 	}
	// }

	wg.Wait()

	printResult(yearAndCpu)
}

type ByAll []Parameter

func (a ByAll) Len() int           { return len(a) }
func (a ByAll) Swap(i, j int)      { a[i], a[j] = a[j], a[i] }
func (a ByAll) Less(i, j int) bool { return a[i].TotalCost < a[j].TotalCost }

func printResult(yearAndCpu []Parameter) {
	sort.Sort(ByAll(yearAndCpu))
	for _, elem := range yearAndCpu {
		fmt.Printf("i: %f, j: %f, k1: %f, k2: %f , cpu: %f, mem: %f",
			elem.I, elem.J, elem.K1, elem.K2, elem.C, elem.M)
		fmt.Printf("min cost by all: %d, %d, %d\n", elem.Cost1, elem.Cost2, elem.TotalCost)
	}
}
