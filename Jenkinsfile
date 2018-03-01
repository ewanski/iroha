// Overall pipeline looks like the following
//               
//   |--Linux-----|----Debug
//   |            |----Release 
//   |    OR
//   |           
//-- |--Linux ARM-|----Debug
//   |            |----Release
//   |    OR
//   |
//   |--MacOS-----|----Debug
//   |            |----Release
properties([parameters([
    choice(choices: 'Debug\nRelease', description: '', name: 'BUILD_TYPE'),
    booleanParam(defaultValue: true, description: '', name: 'Linux'),
    booleanParam(defaultValue: false, description: '', name: 'ARMv7'),
    booleanParam(defaultValue: false, description: '', name: 'ARMv8'),
    booleanParam(defaultValue: false, description: '', name: 'MacOS'),
    booleanParam(defaultValue: false, description: 'Whether build docs or not', name: 'Doxygen'),
    booleanParam(defaultValue: false, description: 'Whether build Java bindings', name: 'JavaBindings'),
    booleanParam(defaultValue: false, description: 'Whether build Python bindings', name: 'PythonBindings'),
    booleanParam(defaultValue: false, description: 'Whether build bindings only w/o Iroha itself', name: 'BindingsOnly'),
    string(defaultValue: '4', description: 'How much parallelism should we exploit. "4" is optimal for machines with modest amount of memory and at least 4 cores', name: 'PARALLELISM')])])

pipeline {
    environment {
        CCACHE_DIR = '/opt/.ccache'
        SORABOT_TOKEN = credentials('SORABOT_TOKEN')
        SONAR_TOKEN = credentials('SONAR_TOKEN')
        CODECOV_TOKEN = credentials('CODECOV_TOKEN')
        DOCKERHUB = credentials('DOCKERHUB')
        DOCKER_BASE_IMAGE_DEVELOP = 'hyperledger/iroha:develop'
        DOCKER_BASE_IMAGE_RELEASE = 'hyperledger/iroha:latest'

        IROHA_NETWORK = "iroha-${GIT_COMMIT}-${BUILD_NUMBER}"
        IROHA_POSTGRES_HOST = "pg-${GIT_COMMIT}-${BUILD_NUMBER}"
        IROHA_POSTGRES_USER = "pg-user-${GIT_COMMIT}"
        IROHA_POSTGRES_PASSWORD = "${GIT_COMMIT}"
        IROHA_REDIS_HOST = "redis-${GIT_COMMIT}-${BUILD_NUMBER}"
        IROHA_POSTGRES_PORT = 5432
        IROHA_REDIS_PORT = 6379

        LCOV_ALREADY_BUILT = 0
    }

    options {
        buildDiscarder(logRotator(numToKeepStr: '20'))
    }

    agent any
    stages {
        stage ('Stop same job builds') {
            agent { label 'master' }
            steps {
                script {
                    // Stop same job running builds if any
                    def builds = load ".jenkinsci/cancel-builds-same-job.groovy"
                    builds.cancelSameCommitBuilds()
                }
            }
        }
        stage('Build Debug') {
            when {
                allOf {
                    expression { params.BUILD_TYPE == 'Debug' }
                    expression { return !params.BindingsOnly }
                }                
            }
            parallel {
                stage ('Linux') {
                    when { expression { return params.Linux } }
                    agent { label 'linux && x86_64' }
                    steps {
                        script {
                            debugBuild = load ".jenkinsci/debug-build.groovy"
                            debugBuild.doDebugBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }
                }
                stage('ARMv7') {
                    when { expression { return params.ARMv7 } }
                    agent { label 'armv7' }
                    steps {
                        script {
                            def debugBuild = load ".jenkinsci/debug-build.groovy"
                            debugBuild.doDebugBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }
                }
                stage('ARMv8') {
                    when { expression { return params.ARMv8 } }
                    agent { label 'armv8' }
                    steps {
                        script {
                            def debugBuild = load ".jenkinsci/debug-build.groovy"
                            debugBuild.doDebugBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }
                }
                stage('MacOS'){
                    when { expression { return params.MacOS } }
                    agent { label 'mac' }
                    steps {
                        script {
                            def scmVars = checkout scm
                            env.IROHA_VERSION = "0x${scmVars.GIT_COMMIT}"
                            env.IROHA_HOME = "/opt/iroha"
                            env.IROHA_BUILD = "${env.IROHA_HOME}/build"
                            env.CCACHE_DIR = "${env.IROHA_HOME}/.ccache"

                            sh """
                                ccache --version
                                ccache --show-stats
                                ccache --zero-stats
                                ccache --max-size=5G
                            """
                            sh """
                                cmake \
                                  -DCOVERAGE=ON \
                                  -DTESTING=ON \
                                  -H. \
                                  -Bbuild \
                                  -DCMAKE_BUILD_TYPE=${params.BUILD_TYPE} \
                                  -DIROHA_VERSION=${env.IROHA_VERSION}
                            """
                            sh "cmake --build build -- -j${params.PARALLELISM}"
                            sh "ccache --show-stats"
                            
                            // TODO: replace with upload to artifactory server
                            // only develop branch
                            if ( env.BRANCH_NAME == "develop" ) {
                                //archive(includes: 'build/bin/,compile_commands.json')
                            }
                        }
                    }
                    post {
                        always {
                            script {
                                cleanWs()
                            }
                        }
                    }
                }
            }
        }
        stage('Build Release') {
            when {
                allOf {
                    expression { params.BUILD_TYPE == 'Release' }
                    expression { return ! params.BindingsOnly }
                }                
            }
            parallel {
                stage('Linux') {
                    when { expression { return params.Linux } }
                    agent { label 'linux && x86_64' }
                    steps {
                        script {
                            def releaseBuild = load ".jenkinsci/release-build.groovy"
                            releaseBuild.doReleaseBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }
                }
                stage('ARMv7') {
                    when { expression { return params.ARMv7 } }
                    agent { label 'armv7' }
                    steps {
                        script {
                            def releaseBuild = load ".jenkinsci/release-build.groovy"
                            releaseBuild.doReleaseBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }                        
                }
                stage('ARMv8') {
                    when { expression { return params.ARMv8 } }
                    agent { label 'armv8' }
                    steps {
                        script {
                            def releaseBuild = load ".jenkinsci/release-build.groovy"
                            releaseBuild.doReleaseBuild()
                        }
                    }
                    post {
                        always {
                            script {
                                def cleanup = load ".jenkinsci/docker-cleanup.groovy"
                                cleanup.doDockerCleanup()
                                cleanWs()
                            }
                        }
                    }                        
                }
                stage('MacOS') {
                    when { expression { return params.MacOS } }                        
                    steps {
                        script {
                            def scmVars = checkout scm
                            env.IROHA_VERSION = "0x${scmVars.GIT_COMMIT}"
                            env.IROHA_HOME = "/opt/iroha"
                            env.IROHA_BUILD = "${env.IROHA_HOME}/build"
                            env.CCACHE_DIR = "${env.IROHA_HOME}/.ccache"

                            sh """
                                ccache --version
                                ccache --show-stats
                                ccache --zero-stats
                                ccache --max-size=5G
                            """    
                            sh """
                                cmake \
                                  -H. \
                                  -Bbuild \
                                  -DCMAKE_BUILD_TYPE=${params.BUILD_TYPE} \
                                  -DIROHA_VERSION=${env.IROHA_VERSION}
                            """
                            sh "cmake --build build -- -j${params.PARALLELISM}"
                            sh "ccache --show-stats"
                            
                            // TODO: replace with upload to artifactory server
                            // only develop branch
                            if ( env.BRANCH_NAME == "develop" ) {
                                //archive(includes: 'build/bin/,compile_commands.json')
                            }
                        }
                    }
                }
            }
        }
        // stage('SonarQube') {
        //     when { expression { params.BUILD_TYPE == 'Release' } }
        //     steps {
        //         sh """
        //             if [ -n ${SONAR_TOKEN} ] && \
        //               [ -n ${BUILD_TAG} ] && \
        //               [ -n ${BRANCH_NAME} ]; then
        //               sonar-scanner \
        //                 -Dsonar.login=${SONAR_TOKEN} \
        //                 -Dsonar.projectVersion=${BUILD_TAG} \
        //                 -Dsonar.branch=${BRANCH_NAME}
        //             else
        //               echo 'required env vars not found'
        //             fi
        //         """
        //     }
        // }
        stage('Build docs') {
            // build docs on any vacant node. Prefer `x86_64` over
            // others as nodes are more powerful
            agent { label 'x86_64 || arm' }
            when {
                allOf {
                    expression { return params.Doxygen }
                    expression { BRANCH_NAME ==~ /(master|develop)/ }
                }
            }
            steps {
                script {
                    def doxygen = load ".jenkinsci/doxygen.groovy"
                    docker.image("${env.DOCKER_IMAGE}").inside {
                        def scmVars = checkout scm
                        doxygen.doDoxygen()
                    }
                }
            }
        }
        stage('Build bindings') {
            agent { label 'x86_64' }
            when {
                anyOf {
                    expression { return params.BindingsOnly }
                    expression { return params.PythonBindings }
                    expression { return params.JavaBindings }
                }
            }
            steps {
                script {
                    def bindings = load ".jenkinsci/bindings.groovy"
                    def platform = sh(script: 'uname -m', returnStdout: true).trim()
                    sh "curl -L -o /tmp/${env.GIT_COMMIT}/Dockerfile --create-dirs https://raw.githubusercontent.com/hyperledger/iroha/${env.GIT_COMMIT}/docker/develop/${platform}/Dockerfile"
                    iC = docker.build("hyperledger/iroha-develop:${GIT_COMMIT}-${BUILD_NUMBER}", "-f /tmp/${env.GIT_COMMIT}/Dockerfile /tmp/${env.GIT_COMMIT} --build-arg PARALLELISM=${PARALLELISM}")
                    sh "rm -rf /tmp/${env.GIT_COMMIT}"
                    iC.inside {
                        def scmVars = checkout scm
                        bindings.doBindings()
                    }
                }
            }
        }
    }
}
