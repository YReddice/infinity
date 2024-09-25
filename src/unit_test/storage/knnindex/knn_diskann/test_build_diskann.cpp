// Copyright(C) 2023 InfiniFlow, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <gtest/gtest.h>

import stl;
import base_test;
import infinity_exception;
import knn_diskann;
import internal_types;
import file_system;
import file_system_type;
import local_file_system;
import file_system_type;
import index_base;
import diskann_index_data;


using namespace infinity;

class DiskAnnTest : public BaseTest {
public:
    const std::string save_dir_ = GetFullTmpDir();

    template<typename DiskAnnIndexDataType>
    void TestCreateIndex() {
        // const std::string save_dir_ = tmp_dir + "/diskann_test";

        std::cout << "Testing Simple Create Index" << std::endl;
        u32 dim = 100;
        u32 num_points = 100000;
        u32 R = 32;
        u32 L = 100;
        u32 num_pq_chunks = 4;
        u32 num_parts = 1;
        u32 num_centers = 25;

        Vector<SizeT> labels(num_points);
        for (u32 i = 0; i < num_points; i++) {
            labels[i] = i;
        }
        auto data = MakeUnique<f32[]>(dim * num_points);
        for (u32 i = 0; i < num_points; i++) {
            u32 non_zero_dim = i / 1000;
            data[i * dim + non_zero_dim] = 1.0f;
        }

        LocalFileSystem fs;
        std::string data_file_path = save_dir_ + "/data.bin";
        std::string mem_index_path = save_dir_ + "/mem_index.bin";
        std::string index_file_path = save_dir_ + "/index.bin";
        std::string pqCompressed_data_path = save_dir_ + "/pqCompressed_data.bin";
        std::string sample_data_path = save_dir_;
        auto [data_file_handler, status] = fs.OpenFile(data_file_path, FileFlags::WRITE_FLAG | FileFlags::CREATE_FLAG, FileLockType::kNoLock);
        data_file_handler->Write(data.get(), sizeof(f32) * dim * num_points);

        {
            auto disk_data = DiskAnnIndexDataType::Make(dim, num_points, R, L, num_pq_chunks, num_parts, num_centers);
            
            disk_data->BuildIndex(dim, num_points, labels, fs, Path(data_file_path), Path(mem_index_path), Path(index_file_path), Path(pqCompressed_data_path), Path(sample_data_path));

            std::cout << "Index built successfully" << std::endl;
        }

    }
};


TEST_F(DiskAnnTest, test1) {
    using DiskAnnIndexData = DiskAnnIndexData<f32, SizeT, MetricType::kMetricL2>;
    TestCreateIndex<DiskAnnIndexData>();
}