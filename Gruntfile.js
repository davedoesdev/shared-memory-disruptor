"use strict";

module.exports = function (grunt)
{
    grunt.initConfig(
    {
        jshint: {
            src: [ 'lib/**/*.js', 'test/**/*.js' ],
            options: {
                node: true,
                esversion: 6
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

            cover_reset: {
                cmd: 'lcov --rc lcov_branch_coverage=1 --zerocounters --directory build'
            },

            cover: {
                cmd: "./node_modules/.bin/nyc -x Gruntfile.js -x 'test/**' node --napi-modules ./node_modules/.bin/grunt test"
            },

            cover_lcov: {
                cmd: "./node_modules/.bin/nyc report -r lcovonly && lcov --rc lcov_branch_coverage=1 --capture --directory build --output-file coverage/lcov_addon.info && lcov --rc lcov_branch_coverage=1 --add-tracefile coverage/lcov.info --add-tracefile coverage/lcov_addon.info --output-file coverage/lcov.info && rm -f coverage/lcov_addon.info && lcov --rc lcov_branch_coverage=1 --remove coverage/lcov.info '/usr/*' $PWD/'node_modules/*' --output-file coverage/lcov.info"
            },

            cover_report: {
                cmd: 'genhtml --rc lcov_branch_coverage=1 -o coverage/lcov-report coverage/lcov.info'
            },

            cover_check: {
                cmd: "if lcov --rc lcov_branch_coverage=1 --list coverage/lcov.info | grep -o '[0-9.]\\+%' | grep -qv 100%; then exit 1; fi"
            },

            coveralls: {
                cmd: 'cat coverage/lcov.info | ./node_modules/.bin/coveralls'
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
                                    'exec:cover_reset',
                                    'exec:cover',
                                    'exec:cover_lcov',
                                    'exec:cover_report',
                                    'exec:cover_check']);
};
