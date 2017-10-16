#define BOOST_TEST_MODULE test module name
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>

int main(int argc, char* argv[], char* envp[])
{
    (void)(envp);
    return boost::unit_test::unit_test_main( &init_unit_test, argc, argv );
}
