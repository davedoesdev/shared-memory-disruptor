"use strict";

module.exports = function (grunt)
{
    grunt.initConfig(
    {
        jshint: {
            src: [ 'lib/**/*.js', 'test/**/*.js' ],
            options: {
                node: true,
                esversion: 6,
                expr: true
            }
        },

        mochaTest: {
            src: 'test/*.js',
            options: {
                bail: true
            }
        },

        exec: {
            build: {
                cmd: 'node-gyp build --debug'
            },

			cover_build: {
                cmd: 'node-gyp rebuild --debug --coverage=true'
			},

            cover_init: {
                cmd: 'lcov --rc lcov_branch_coverage=0 --zerocounters --directory build && lcov --rc lcov_branch_coverage=0 --capture --init --directory build -o coverage/lcov_base.info'
            },

            cover: {
                cmd: "./node_modules/.bin/nyc -x Gruntfile.js -x 'test/**' node --napi-modules ./node_modules/.bin/grunt test"
            },

            cover_lcov: {
                cmd: "./node_modules/.bin/nyc report -r lcovonly && lcov --rc lcov_branch_coverage=0 --capture --directory build --output-file coverage/lcov_addon.info && lcov --rc lcov_branch_coverage=1 --add-tracefile coverage/lcov.info --add-tracefile coverage/lcov_base.info --add-tracefile coverage/lcov_addon.info --output-file coverage/lcov.info && lcov --rc lcov_branch_coverage=1 --remove coverage/lcov.info '/usr/*' $PWD/'node_modules/*' --output-file coverage/lcov.info"
            },

            cover_report: {
                cmd: 'genhtml --rc lcov_branch_coverage=1 --demangle-cpp -o coverage/lcov-report coverage/lcov.info'
            },

            cover_check: {
                // lines% functions% branches%
                // Functions aren't 100% because gcov says
                // DisruptorAsyncWorker<>::~DisruptorAsyncWorker() isn't called
                // even though (a) the child classes' destructors are and
                // (b) if I define them and then log from them, the functions
                // are called. Some kind of bug with gcov and virtual
                // destructors, maybe in combination with templates.
                // Branches for C++ are disabled because gcov results are
                // messed up by exceptions.
                cmd: "if [ \"$(lcov --rc lcov_branch_coverage=1 --list coverage/lcov.info | grep Total | grep -o '[0-9.]\\+%' | tr '\\n' ' ')\" != '100% 97.3% 100% ' ]; then exit 1; fi"
            },

            coveralls: {
                cmd: 'cat coverage/lcov.info | ./node_modules/.bin/coveralls'
            },

            documentation: {
                cmd: './node_modules/.bin/documentation build -f html -o docs docs.js'
            }
        }
    })

    grunt.loadNpmTasks('grunt-contrib-jshint');
    grunt.loadNpmTasks('grunt-mocha-test');
    grunt.loadNpmTasks('grunt-exec');

    grunt.registerTask('lint', 'jshint');
    grunt.registerTask('build', 'exec:build');
    grunt.registerTask('test', 'mochaTest');
    grunt.registerTask('coverage', ['exec:cover_build',
                                    'exec:cover_init',
                                    'exec:cover',
                                    'exec:cover_lcov',
                                    'exec:cover_report',
                                    'exec:cover_check']);
    grunt.registerTask('docs', 'exec:documentation');
};
