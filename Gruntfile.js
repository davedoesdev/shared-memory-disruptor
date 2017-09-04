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
			cover_build: {
                cmd: 'node-gyp rebuild --coverage=true'
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
                cmd: './node_modules/.bin/nyc check-coverage --statements 100 --branches 100 --functions 100 --lines 100'
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
    grunt.registerTask('test', 'mochaTest');
    grunt.registerTask('coverage', ['exec:cover_build',
                                    'exec:cover',
                                    'exec:cover_lcov',
                                    'exec:cover_report',
                                    'exec:cover_check']);
};
