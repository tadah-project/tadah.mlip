#!/bin/bash
compare_files() {
    local file1="$1"
    local file2="$2"

    paste "$file1" "$file2" | awk '
    NR > 1 {
        true_energy_1 = sprintf("%.5f", $2)
        pred_energy_1 = sprintf("%.5f", $3)
        true_energy_2 = sprintf("%.5f", $5)
        pred_energy_2 = sprintf("%.5f", $6)
        
        if (true_energy_1 != true_energy_2 || pred_energy_1 != pred_energy_2) {
            print "Error: Difference at line", NR-1, "\nFile1:", $2, $3, "\nFile2:", $5, $6 > "/dev/stderr"
            status = 2
        }
    }
    END {if (status == 2) exit 2}
    ' 
}

# Example usage:
compare_files energy.pred EXPECTED_OUTPUT/energy.pred
compare_files forces.pred EXPECTED_OUTPUT/forces.pred
compare_files stress.pred EXPECTED_OUTPUT/stress.pred
